import argparse
import json
import operator
import os
import sys
import arapi.utils
import arapi.types
import arapi.writer
import arapi.parser
from collections import deque
from collections import OrderedDict

function_template_start = """
        {{
            auto func        = std::make_shared<::agea::reflection::function>();
            auto f           = func.get();

            rtype->m_functions.emplace_back(std::move(func));

            f->name          = "{function}";
"""

function_template_end = """
        }
"""


class agea_range:

  def __init__(self, m, o: int, c: int):
    self.module = m
    self.offset = int(o)
    self.count = int(c)
    self.dependency = []
    self.has_render = False


class agea_range_list:

  def __init__(self, p: str, a: str):
    self.ranges = []
    self.path = os.path.join(os.path.dirname(p), "modules")
    self.modules_root = os.path.join(os.path.dirname(p))
    self.active_module_path = os.path.join(a, "engine", "active_modules.cpp")
    self.active_module_include_path = os.path.join(a, "engine", "active_modules.h")
    self.handled = {}
    self.graph = {}
    self.order = deque()

  def load(self):
    root = agea_range("0000", 0, 0)
    root.has_render = True
    self.ranges.append(root)
    if os.path.exists(self.path):
      with open(self.path, "r") as file:
        lines = file.readlines()
        for l in lines:
          tokens = l.strip().split(":")

          if tokens[0] == "root":
            self.ranges[0].offset = 0
            self.ranges[0].count = int(tokens[2])
          else:
            self.ranges.append(agea_range(tokens[0], tokens[1], tokens[2]))

            self.read_config(self.ranges[-1])

      self.ranges.sort(key=operator.attrgetter("module"))

  def find(self, module_name):
    for i in range(len(self.ranges)):
      if module_name == self.ranges[i].module:
        return i
    return -1

  def read_config(self, r: agea_range):
    module_path = os.path.join(self.modules_root, r.module, "ar/module")

    with open(module_path) as file:
      file_contents = json.loads(file.read())
      r.dependency = file_contents["dependency"]
      r.has_render = file_contents["has_render"]

  def add(self, module_name, count):
    r = agea_range(module_name, 0, count)

    self.read_config(r)

    self.ranges.append(r)
    self.ranges.sort(key=operator.attrgetter("module"))

  def update(self, module_name, types_count):
    rearrange = 0
    if module_name == "root":
      if types_count > self.ranges[0].count:
        self.ranges[0].count = ((types_count // 16) + 1) * 16
        offset = self.ranges[0].count
    else:
      rearrange = self.find(module_name)
      if rearrange == -1:
        self.add(module_name, ((types_count // 16) + 1) * 16)
        rearrange = self.find(module_name)

    if rearrange < 1:
      return rearrange

    for i in range(rearrange, len(self.ranges)):
      offset = self.ranges[i - 1].count + self.ranges[i - 1].offset
      self.ranges[i].offset = offset

    return rearrange

  def gen_register(self):
    self.ranges[0].module = "root"

    for r in self.ranges:
      if r.module not in self.graph:
        self.graph[r.module] = []

      for i in r.dependency:
        if i not in self.graph:
          self.graph[i] = []

        self.graph[i].append(r)

    self.handle(self.ranges[0])

  def handle(self, r):
    if r.module not in self.handled:
      for i in self.graph[r.module]:
        self.handle(i)

      self.handled[r.module] = True
      self.order.appendleft(r)

  def save(self):
    range_desc = "{module_name}:{offset}:{count}"

    mod_include_template = """#include "{0}/package.h"
"""
    mod_register_template = """    glob::module_manager::getr().register_module<{0}::{0}_module>();
"""

    mod_render_include_template = """#include "{0}/render/{0}_module_render_bridge.h"
"""
    mod_render_register_template = """    glob::module_manager::getr().register_module<{0}::{0}_module_render_bridge>();
"""

    with open(self.path, "w") as file:
      file.write(
          range_desc.format(
              module_name="root",
              offset=self.ranges[0].offset,
              count=self.ranges[0].count,
          ))

      file.write("\n")

      for r in range(1, len(self.ranges)):
        r = self.ranges[r]
        file.write(range_desc.format(module_name=r.module, offset=r.offset, count=r.count))

        file.write("\n")

    self.gen_register()

    mod_includes = "#pragma once\n"
    mod_register = ""

    for m in self.order:
      mod_includes += mod_include_template.format(m.module)
      mod_register += mod_register_template.format(m.module)

      if m.has_render:
        mod_includes += mod_render_include_template.format(m.module)
        mod_register += mod_render_register_template.format(m.module)


def write_functions(context: arapi.types.file_context, func: arapi.types.agea_function,
                    current_class):
  context.content += function_template_start.format(function=func.name)

  context.content += function_template_end


def gen_id(type: arapi.types.agea_type, module_name: str):
  type_name_tokens = type.name.split('::')
  type_name = '_'
  if len(type_name_tokens) > 1:

    for token in type_name_tokens:
      if token not in (module_name, 'std', 'agea', ''):
        type_name += '_'
        type_name += token
  else:
    type_name = '__' + type_name_tokens[0]

  type.id = module_name + type_name


def update_global_ids(fc: arapi.types.file_context):

  lines = []

  global_file = os.path.join(fc.global_dir, "type_ids.ar.h")
  if not os.path.exists(global_file):
    with open(global_file, "w+") as gf:

      fl = f"""#pragma once
// clang-format off

namespace agea {{
  enum {{
// block start zzero
    agea__total_supported_types_number,
    agea__invalid_type_id = agea__total_supported_types_number
// block end zzero
  }};
}}
"""

      gf.write(fl)

  with open(global_file, "r+") as gf:
    lines = gf.readlines()

    mapping = OrderedDict()

    for i in range(0, len(lines)):
      start_index = None
      end_index = None
      line = lines[i].strip()
      if line.startswith("// block start "):
        start_index = i
        while not line.startswith("// block end "):
          i = i + 1
          if i == len(lines):
            exit(-1)

          line = lines[i].strip()
        end_index = i
        tokens = line.split(" ")
        mapping[tokens[3]] = (start_index, end_index)

    start_index = None
    end_index = None
    for item in mapping.items():
      if item[0] == fc.module_name:
        start_index = item[1][0]
        end_index = item[1][1] + 1
        break
      elif item[0] > fc.module_name:
        start_index = item[1][0]
        end_index = item[1][0]
        break

    new_ids = []
    for t in fc.types:
      new_ids.append(f"    {fc.module_name}__{t.name},\n")

    new_ids.sort()
    new_ids.insert(0, f"// block start {fc.module_name}\n")
    new_ids.append(f"// block end {fc.module_name}\n")
    lines[start_index:end_index] = new_ids

    # Write the result back to the file
    with open(global_file, "w", encoding="utf-8") as f:
      f.writelines(lines)


def update_dependancy_tree(fc: arapi.types.file_context):

  lines = []

  global_file = os.path.join(fc.global_dir, "dependency_tree.ar.h")
  if os.path.exists(global_file):
    with open(global_file, "w+") as gf:

      fl = f"""// clang-format off
#pragma once

#include <vector>
#include <utils/id.h>

namespace agea
{{

std::vector<utils::id>
get_dapendency(const utils::id& package_id)
{{
    // block start root
    if (package_id == AID("root"))
    {{
        return {{}};
    }}
    // block end root
    return {{}};
}}

}}  // namespace agea
"""

      gf.write(fl)

  with open(global_file, "r+") as gf:
    lines = gf.readlines()

    mapping = OrderedDict()

    for i in range(0, len(lines)):
      start_index = None
      end_index = None
      line = lines[i].strip()
      if line.startswith("// block start "):
        start_index = i
        while not line.startswith("// block end "):
          i = i + 1
          if i == len(lines):
            exit(-1)

          line = lines[i].strip()
        end_index = i
        tokens = line.split(" ")
        mapping[tokens[3]] = (start_index, end_index)

    start_index = None
    end_index = None
    for item in mapping.items():
      if item[0] == fc.module_name:
        start_index = item[1][0]
        end_index = item[1][1] + 1
        break
      elif item[0] > fc.module_name:
        start_index = item[1][0]
        end_index = item[1][0]
        break
      elif item[0] < fc.module_name:
        start_index = item[1][0]
        end_index = item[1][0]

    new_ids = []
    new_ids.append(f"//    block start {fc.module_name}\n")
    new_ids.append(f"""    if( package_id == AID("{fc.module_name}"))\n""")
    new_ids.append(f"""    {{\n""")
    new_ids.append(f"""        return {{""")
    for d in fc.dependencies:
      new_ids.append(f"""AID("{d}"),""")

    if new_ids[-1][-1] == ",":
      new_ids[-1] = new_ids[-1][:-1]

    new_ids.append(f"""}};\n""")
    new_ids.append(f"""    }};\n""")
    new_ids.append(f"    // block end {fc.module_name}\n")
    lines[start_index:end_index] = new_ids

    # Write the result back to the file
    with open(global_file, "w", encoding="utf-8") as f:
      f.writelines(lines)


def build_package(ar_cfg_path, root_dir, output_dir, module_name, module_namespace):
  print("""AR generator :
      cfg          - {0}
      root         - {1}
      output       - {2}
      package_name - {3}
      namespace    - {4}""".format(ar_cfg_path, root_dir, output_dir, module_name,
                                   module_namespace))
  module_namespace = module_namespace.strip()

  context = arapi.types.file_context(module_name, module_namespace)
  context.output_dir = output_dir
  context.root_dir = root_dir
  context.package_header_dir = os.path.join(output_dir, "packages", module_name, "public",
                                            "include", "packages", module_name)
  context.model_sources_dir = os.path.join(output_dir, "packages", module_name, "private", "model")
  context.model_header_dir = os.path.join(output_dir, "packages", module_name, "public", "include",
                                          "packages", module_name, "model")
  context.render_sources_dir = os.path.join(output_dir, "packages", module_name, "private",
                                            "render")
  context.render_header_dir = os.path.join(output_dir, "packages", module_name, "render", "public")

  context.global_dir = os.path.join(output_dir, "packages/glue/public/include/glue")

  # init folder structure
  for d in [
      context.model_sources_dir, context.model_header_dir, context.render_sources_dir,
      context.render_header_dir, context.package_header_dir, context.global_dir
  ]:
    if not os.path.exists(d):
      os.makedirs(d)

  cfg = open(ar_cfg_path, "r", newline="\n")
  lines = cfg.readlines()
  for f in lines:
    f = arapi.utils.extstrip(f)
    if len(f) > 0:
      file_path = os.path.join(root_dir, f).replace("\\", "/")
      arapi.parser.parse_file(file_path, f, module_name, context)

  context.order_types_by_parent()

  for t in context.types:
    gen_id(t, context.module_name)

  # write object model reflection
  output_file = os.path.join(context.model_sources_dir, f"package.{module_name}.ar.cpp")

  arapi.writer.write_object_model_reflection(output_file, context)

  for t in context.types:
    if t.kind == arapi.types.agea_type_kind.CLASS:
      arapi.writer.write_ar_class_include_file(t, context, output_dir)

  update_global_ids(context)
  update_dependancy_tree(context)
  # write rendering reflection

  arapi.writer.write_ar_package_include_file(context, output_dir)

  if context.render_has_types_overrides or context.render_has_custom_resources:
    output_file = os.path.join(context.render_sources_dir, f"package.{module_name}.render.ar.cpp")
    arapi.writer.write_render_types_reflection(output_file, context)

  #for t in context.types:


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Optional app description")

  parser.add_argument("--type", type=str, help="type")

  parser.add_argument("--config", type=str, help="cfg")

  parser.add_argument("--source", type=str, help="src")

  parser.add_argument("--output", type=str, help="output")

  parser.add_argument("--package_name", type=str, help="package name")

  parser.add_argument("--namespace", type=str, help="namespace")

  args = parser.parse_args()

  if args.type == "package":
    build_package(args.config, args.source, args.output, args.package_name, args.namespace)
  else:
    print("Wrong arg")
