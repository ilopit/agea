"""AR (Agea Reflection) code generator.

This module is the main entry point for generating reflection code.
It manages module ranges, dependencies, type IDs, and orchestrates
the code generation process.
"""
import argparse
import json
import operator
import os
import sys
from collections import OrderedDict, deque
from typing import List, Optional, Tuple, Dict

import arapi.parser
import arapi.types
import arapi.utils
import arapi.writer

# Constants
MODULE_ROOT = "root"
FILE_TYPE_IDS = "type_ids.ar.h"
FILE_DEPENDENCY_TREE = "dependency_tree.ar.h"

# Block markers
BLOCK_START_PREFIX = "// block start "
BLOCK_END_PREFIX = "// block end "

# Namespace tokens to exclude from type IDs
EXCLUDED_NAMESPACE_TOKENS = {MODULE_ROOT, 'std', 'agea', ''}


def gen_id(type_obj: arapi.types.agea_type, module_name: str) -> None:
  """Generate type ID for a type.
    
    Args:
        type_obj: Type to generate ID for
        module_name: Name of the module
    """
  type_name_tokens = type_obj.name.split('::')
  type_name = '_'

  if len(type_name_tokens) > 1:
    for token in type_name_tokens:
      if token not in EXCLUDED_NAMESPACE_TOKENS:
        type_name += '_'
        type_name += token
  else:
    type_name = '__' + type_name_tokens[0]

  type_obj.id = module_name + type_name


def update_global_ids(fc: arapi.types.file_context) -> None:
  """Update global type IDs file with module types.
    
    Args:
        fc: File context with types to register
    """
  global_file = os.path.join(fc.global_dir, FILE_TYPE_IDS)

  # Create file if it doesn't exist
  if not os.path.exists(global_file):
    with open(global_file, "w+", encoding="utf-8") as gf:
      file_content = """#pragma once
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
      gf.write(file_content)

  # Read and update file
  with open(global_file, "r+", encoding="utf-8") as gf:
    lines = gf.readlines()

  mapping = OrderedDict()

  # Find all blocks
  for i in range(len(lines)):
    line = lines[i].strip()
    if line.startswith(BLOCK_START_PREFIX):
      start_index = i
      while not line.startswith(BLOCK_END_PREFIX):
        i += 1
        if i == len(lines):
          exit(-1)
        line = lines[i].strip()
      end_index = i
      tokens = line.split(" ")
      mapping[tokens[3]] = (start_index, end_index)

  # Find insertion point for this module
  start_index = None
  end_index = None
  for module_name, (block_start, block_end) in mapping.items():
    if module_name == fc.module_name:
      start_index = block_start
      end_index = block_end + 1
      break
    elif module_name > fc.module_name:
      start_index = block_start
      end_index = block_start
      break

  # Generate new IDs
  new_ids: List[str] = []
  for type_obj in fc.types:
    new_ids.append(f"    {fc.module_name}__{type_obj.name},\n")

  new_ids.sort()
  new_ids.insert(0, f"{BLOCK_START_PREFIX}{fc.module_name}\n")
  new_ids.append(f"{BLOCK_END_PREFIX}{fc.module_name}\n")

  if start_index is not None and end_index is not None:
    lines[start_index:end_index] = new_ids

  # Write the result back to the file
  with open(global_file, "w", encoding="utf-8") as f:
    f.writelines(lines)

def replace_named_blocks(text: str, replacements: dict) -> str:
  """
    Replace sections in `text` that are wrapped by:
        // block start <name>
        ...
        // block end <name>

    Parameters:
        text (str): original file text
        replacements (dict): { "blockname": "new content" }

    Returns:
        str: text with replacements applied
    """
  for name, new_content in replacements.items():
    # Regex that captures:
    #   (start marker) (any content, non-greedy) (end marker)
    pattern = (
        rf"(// block start {re.escape(name)}\s*)"    # group 1: start marker
        r"(.*?)"                                      # group 2: block body
        rf"(\s*// block end {re.escape(name)})"       # group 3: end marker
    )

    # Build replacement text
    replacement = r"\1" + "\n" + new_content + "\n" + r"\3"

    # Perform substitution (DOTALL so '.' matches newlines)
    text, count = re.subn(pattern, replacement, text, flags=re.DOTALL)

    if count == 0:
      print(f"Warning: block '{name}' not found.")

  return text


def update_dependancy_tree(fc: arapi.types.file_context) -> None:
  """Update dependency tree file with module dependencies.
    
    Note: Function name has typo "dependancy" but kept for backward compatibility.
    
    Args:
        fc: File context with dependencies to register
    """
  global_file = os.path.join(fc.global_dir, FILE_DEPENDENCY_TREE)

  # Create file if it doesn't exist
  if not os.path.exists(global_file):
    with open(global_file, "w+", encoding="utf-8") as gf:
      file_content = """// clang-format off
#pragma once

#include <vector>
#include <utils/id.h>

namespace agea
{

std::vector<utils::id>
get_dapendency(const utils::id& package_id)
{
    // block start root
    if (package_id == AID("root"))
    {
        return {};
    }
    // block end root
    return {};
}

}  // namespace agea
"""
      gf.write(file_content)

  # Generate dependency block content for this module
  # Note: replace_blocks_with_sorted_insert adds indentation automatically:
  # - For existing blocks: uses indentation from block start line (4 spaces in template)
  # - For new blocks: uses 4 spaces default
  # So we provide content WITHOUT leading indentation - it will be added by the function
  dependency_lines = []
  dependency_lines.append(f'    if( package_id == AID("{fc.module_name}"))')
  dependency_lines.append("    {")

  if not fc.dependencies:
    dependency_lines.append("      return {};")
  else:
    dep_list = "      return {" + ",".join(f'AID("{dep}")' for dep in fc.dependencies) + "};"
    dependency_lines.append(dep_list)

  dependency_lines.append("    }")

  replacement_content = "\n".join(dependency_lines)

  # Use the new function to replace/insert the block
  replacements = {fc.module_name: replacement_content}
  arapi.utils.replace_blocks_with_sorted_insert(global_file, replacements)


def build_package(ar_cfg_path: str, root_dir: str, output_dir: str, module_name: str,
                  module_namespace: str) -> None:
  """Build a package by generating all reflection code.
    
    Args:
        ar_cfg_path: Path to AR configuration file
        root_dir: Root directory of source files
        output_dir: Output directory for generated files
        module_name: Name of the module/package
        module_namespace: Namespace for the module
    """
  print(f"""AR generator :
      cfg          - {ar_cfg_path}
      root         - {root_dir}
      output       - {output_dir}
      package_name - {module_name}
      namespace    - {module_namespace}""")

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

  # Initialize folder structure
  directories = [
      context.model_sources_dir, context.model_header_dir, context.render_sources_dir,
      context.render_header_dir, context.package_header_dir, context.global_dir
  ]

  for directory in directories:
    if not os.path.exists(directory):
      os.makedirs(directory)

  # Parse configuration file and process all files
  with open(ar_cfg_path, "r", newline="\n", encoding="utf-8") as cfg:
    lines = cfg.readlines()
    for line in lines:
      file_path_relative = arapi.utils.extstrip(line)
      if len(file_path_relative) > 0:
        file_path = os.path.join(root_dir, file_path_relative).replace("\\", "/")
        arapi.parser.parse_file(file_path, file_path_relative, module_name, context)

  context.order_types_by_parent()

  # Generate IDs for all types
  for type_obj in context.types:
    gen_id(type_obj, context.module_name)

  # Write object model reflection
  output_file = os.path.join(context.model_sources_dir, f"package.{module_name}.ar.cpp")
  arapi.writer.write_object_model_reflection(output_file, context)

  # Write class include files
  for type_obj in context.types:
    if type_obj.kind == arapi.types.agea_type_kind.CLASS:
      arapi.writer.write_ar_class_include_file(type_obj, context, output_dir)

  # Update global files
  update_global_ids(context)
  update_dependancy_tree(context)

  # Write package include file
  arapi.writer.write_ar_package_include_file(context, output_dir)

  # Write rendering reflection if needed
  if context.render_has_types_overrides or context.render_has_custom_resources:
    output_file = os.path.join(context.render_sources_dir, f"package.{module_name}.render.ar.cpp")
    arapi.writer.write_render_types_reflection(output_file, context)


def main() -> None:
  """Main entry point for AR generator."""
  parser = argparse.ArgumentParser(description="AR (Agea Reflection) code generator")

  parser.add_argument("--type", type=str, help="Generation type (e.g., 'package')")
  parser.add_argument("--config", type=str, help="Configuration file path")
  parser.add_argument("--source", type=str, help="Source directory")
  parser.add_argument("--output", type=str, help="Output directory")
  parser.add_argument("--package_name", type=str, help="Package name")
  parser.add_argument("--namespace", type=str, help="Namespace")

  args = parser.parse_args()

  if args.type == "package":
    build_package(args.config, args.source, args.output, args.package_name, args.namespace)
  else:
    print("Wrong arg")


if __name__ == "__main__":
  main()
