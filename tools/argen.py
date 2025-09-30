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

  if not os.path.exists(fc.global_file):
    with open(fc.global_file, "w+") as gf:

      fl = f"""#pragma once
// clang-format off

namespace agea {{
  enum {{
// root types start
// root types end
    agea__total_supported_types_number,
    agea__invalid_type_id = agea__total_supported_types_number
  }};
}}
"""

      gf.write(fl)

  with open(fc.global_file, "r+") as gf:
    existing_ids = []
    lines = gf.readlines()

  add_mode = 0

  before = []
  existing_ids = []
  after = []
  for line in lines:

    if line.startswith(f"// {fc.module_name} types start"):
      add_mode = 1
      before.append(line)
      continue

    if line.startswith(f"// {fc.module_name} types end"):
      add_mode = 2
      after.append(line)
      continue

    if add_mode == 0:
      before.append(line)
    elif add_mode == 1:
      existing_ids.append(line.strip().replace(',',''))
    elif add_mode == 2:
      after.append(line)

  existing_ids.sort()

  new_ids = []
  for t in fc.types:
    new_ids.append(t.id)

  new_ids.sort()

  if new_ids != existing_ids:
    with open(fc.global_file, "w+") as gf:
      gf.writelines(before)

      for ni in new_ids:
        gf.write(f"    {ni},\n")
      gf.writelines(after)


def build_package(ar_cfg_path, root_dir, output_dir, module_name, module_namespace):
  print("""SOLing:
      cfg          - {0}
      root         - {1}
      output       - {2}
      package_name - {3}
      namespace    - {4}""".format(ar_cfg_path, root_dir, output_dir, module_name,
                                   module_namespace))
  module_namespace = module_namespace.strip()

  context = arapi.types.file_context(module_name, module_namespace)
  context.output_dir = output_dir
  context.private_dir = os.path.join(output_dir, "packages", module_name, "private")
  context.public_dir = os.path.join(output_dir, "packages", module_name, "public")
  context.global_file = os.path.join(output_dir, "packages", "global", "type_ids.ar.h")

  if not os.path.exists(context.private_dir):
    os.mkdir(context.private_dir)

  if not os.path.exists(context.public_dir):
    os.mkdir(context.public_dir)

  global_dir = os.path.join(output_dir, "packages", "global")
  if not os.path.exists(global_dir):
    os.mkdir(global_dir)

  context.has_custom_types = os.path.exists(
      os.path.join(root_dir, "include", "packages", module_name, "types_custom.h"))

  context.has_custom_properties = os.path.exists(
      os.path.join(root_dir, "include", "packages", module_name, "properties_custom.h"))

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

  output_file = os.path.join(context.private_dir, f"package.{module_name}.ar.cpp")

  arapi.writer.write_module_reflection(output_file, context)

  for t in context.types:
    if t.kind == arapi.types.agea_type_kind.CLASS:
      arapi.writer.write_ar_class_include_file(t, context, output_dir)

  update_global_ids(context)


def bind_packages(source: str, output: str):
  registered_packages = ""
  packages_includes = ""

  for d in os.listdir(source):
    if os.path.isdir(os.path.join(source, d)):
      registered_packages += f"    ::agea::glob::package_manager::getr().register_static_package({d}::package::instance());"

      packages_includes += f"#include <packages/root/package.{d}.h>\n"

  packages_glue = f"""#include <engine/agea_engine.h>
#include <core/package_manager.h>

{packages_includes}

namespace agea
{{
void
vulkan_engine::register_packages()
{{
{registered_packages}        
}}  
}}
"""

  fd = open(os.path.join(output, "engine", "packages_glue.ar.cpp"), "w")
  fd.write(packages_glue)


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Optional app description")

  parser.add_argument("--type", type=str, help="type")

  parser.add_argument("--config", type=str, help="cfg")

  parser.add_argument("--source", type=str, help="src")

  parser.add_argument("--output", type=str, help="output")

  parser.add_argument("--package_name", type=str, help="package name")

  parser.add_argument("--namespace", type=str, help="namespace")

  args = parser.parse_args()

  if args.type == "bind":
    bind_packages(args.source, args.output)
  elif args.type == "package":
    build_package(args.config, args.source, args.output, args.package_name, args.namespace)
  else:
    print("Wrong arg")
