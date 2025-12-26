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

  # Write types builder header (for tests that need to instantiate the builder)
  types_builder_header = os.path.join(context.package_header_dir,
                                      f"package.{module_name}.types_builder.ar.h")
  arapi.writer.write_types_builder_header(types_builder_header, context)

  # Write class include files
  for type_obj in context.types:
    if type_obj.kind == arapi.types.agea_type_kind.CLASS:
      arapi.writer.write_ar_class_include_file(type_obj, context, output_dir)

  # Update global files
  arapi.writer.update_global_ids(context)
  arapi.writer.update_dependancy_tree(context)

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
