"""Parser module for KRYGA reflection system.

This module parses C++ header files annotated with KRYGA macros to extract
type information, properties, functions, and other metadata.
"""
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Tuple, Optional

import arapi.types
import arapi.utils

__all__ = [
    # Exceptions
    'ParserError',
    'InvalidBoolValueError',
    'InvalidPropertyError',
    # Main API
    'parse_file',
    'parse_attributes',
    'is_bool',
    'extract_type_config',
]

# Constants
INCLUDE_PREFIX = "include/"
INCLUDE_PREFIX_LEN = len(INCLUDE_PREFIX)

# KRYGA macro markers
MACRO_MODEL_OVERRIDES = "KRG_ar_model_overrides()"
MACRO_RENDER_OVERRIDES = "KRG_ar_render_overrides()"
MACRO_EDITOR_OVERRIDES = "KRG_ar_editor_overrides()"
MACRO_CLASS = "KRG_ar_class"
MACRO_STRUCT = "KRG_ar_struct"
MACRO_FUNCTION = "KRG_ar_function"
MACRO_PROPERTY = "KRG_ar_property"
MACRO_CTOR = "KRG_ar_ctor"
MACRO_EXTERNAL_TYPE = "KRG_ar_external_type("
MACRO_CONSTRUCT_PARAMS = "KRG_gen_construct_params"
MACRO_PACKAGE = "KRG_ar_package"

# Property metadata keys
PROP_KEY_CATEGORY = "category"
PROP_KEY_SERIALIZABLE = "serializable"
PROP_KEY_ACCESS = "access"
PROP_KEY_DEFAULT = "default"
PROP_KEY_GPU_DATA = "gpu_data"
PROP_KEY_GPU_TEXTURE_SLOT = "gpu_texture_slot"
PROP_KEY_COPYABLE = "copyable"
PROP_KEY_UPDATABLE = "updatable"
PROP_KEY_REF = "ref"
PROP_KEY_INVALIDATES = "invalidates"
PROP_KEY_CHECK = "check"
PROP_KEY_HINT = "hint"
PROP_KEY_PROPERTY_SAVE_HANDLER = "property_save_handler"
PROP_KEY_PROPERTY_LOAD_HANDLER = "property_load_handler"
PROP_KEY_PROPERTY_COMPARE_HANDLER = "property_compare_handler"
PROP_KEY_PROPERTY_COPY_HANDLER = "property_copy_handler"
PROP_KEY_PROPERTY_SNAPSHOT_HANDLER = "property_snapshot_handler"
PROP_KEY_PROPERTY_INSTANTIATE_HANDLER = "property_instantiate_handler"
PROP_KEY_INSTANTIATE_MODE = "instantiate"
PROP_KEY_MCP_HINT = "mcp_hint"

# Type config keys
TYPE_KEY_COPY_HANDLER = "copy_handler"
TYPE_KEY_INSTANTIATE_HANDLER = "instantiate_handler"
TYPE_KEY_COMPARE_HANDLER = "compare_handler"
TYPE_KEY_SAVE_HANDLER = "save_handler"
TYPE_KEY_LOAD_HANDLER = "load_handler"
TYPE_KEY_ARCHITYPE = "architype"
TYPE_KEY_BUILT_IN = "built_in"
TYPE_KEY_RENDER_CMD_BUILDER = "render_cmd_builder"
TYPE_KEY_RENDER_CMD_DESTROYER = "render_cmd_destroyer"
TYPE_KEY_RENDER_CMD_TRANSFORM = "render_cmd_transform"
TYPE_KEY_PHYSICS_CMD_BUILDER = "physics_cmd_builder"
TYPE_KEY_PHYSICS_CMD_DESTROYER = "physics_cmd_destroyer"
TYPE_KEY_PHYSICS_CMD_TRANSFORM = "physics_cmd_transform"
TYPE_KEY_JSON_SAVE_HANDLER = "json_save_handler"
TYPE_KEY_JSON_LOAD_HANDLER = "json_load_handler"
TYPE_KEY_MCP_SCHEMA = "mcp_schema"
TYPE_KEY_MCP_HINT = "mcp_hint"

# Package config keys
PKG_KEY_MODEL_TYPES_OVERRIDES = "model.has_types_overrides"
PKG_KEY_MODEL_PROPERTIES_OVERRIDES = "model.has_properties_overrides"
PKG_KEY_RENDER_OVERRIDES = "render.has_overrides"
PKG_KEY_RENDER_RESOURCES = "render.has_resources"
PKG_KEY_EDITOR_OVERRIDES = "editor.has_overrides"
# Note: "dependancies" is intentionally misspelled to match C++ macro API
PKG_KEY_DEPENDENCIES = "dependancies"

# Invalidates values
INVALIDATES_RENDER = "render"
INVALIDATES_TRANSFORM = "transform"

# Check values
CHECK_NOT_SAME = "not_same"

# Valid property access values
VALID_ACCESS_VALUES = frozenset({
    "no", "cpp_readonly", "cpp_only", "cpp_writeonly",
    "script_readonly", "script_writeonly",
    "read_only", "write_only", "all"
})

# Valid boolean string values
VALID_BOOL_VALUES = frozenset({"true", "false"})

# Valid yes/no values
VALID_YES_NO_VALUES = frozenset({"yes", "no"})

# C++ keywords to filter
CXX_KEYWORDS = {"class", "struct", "public", "private", "KRG_ar_external_define", ""}


class ParserError(Exception):
  """Base exception for parser errors."""
  pass


class InvalidBoolValueError(ParserError):
  """Raised when an invalid boolean value is encountered."""
  pass


class InvalidPropertyError(ParserError):
  """Raised when property parsing fails."""
  pass


def parse_attributes(name: str, lines: List[str], index: int,
                     max_index: int) -> Tuple[int, List[str]]:
  """Parse attributes from a multi-line macro call.
    
    Args:
        name: Name of the macro (for error messages)
        lines: List of source lines
        index: Starting line index
        max_index: Maximum line index to search
        
    Returns:
        Tuple of (final_index, list_of_attribute_strings)
    """
  attribute_text = lines[index]

  while index <= max_index and ")" not in lines[index]:
    index += 1
    if index <= max_index:
      attribute_text += lines[index].strip() + " "

  start_pos = attribute_text.find("(") + 1
  end_pos = attribute_text.find(")")

  if end_pos == -1:
    raise ParserError(f"Unclosed parentheses in {name} at line {index}")

  attributes = attribute_text[start_pos:end_pos].split(",")
  attributes = [attr.strip() for attr in attributes if attr.strip()]

  return index, attributes


def is_bool(value: str) -> bool:
  """Convert string to boolean value.

    Args:
        value: String value to convert

    Returns:
        Boolean value

    Raises:
        InvalidBoolValueError: If value is not 'true' or 'false'
    """
  if value == 'true':
    return True
  elif value == 'false':
    return False
  else:
    raise InvalidBoolValueError(f"Expected 'true' or 'false', got '{value}'")


# Mapping from type config keys to attribute names for model overrides
_MODEL_TYPE_ATTR_MAP = {
    TYPE_KEY_COPY_HANDLER: 'copy_handler',
    TYPE_KEY_INSTANTIATE_HANDLER: 'instantiate_handler',
    TYPE_KEY_COMPARE_HANDLER: 'compare_handler',
    TYPE_KEY_SAVE_HANDLER: 'save_handler',
    TYPE_KEY_LOAD_HANDLER: 'load_handler',
    TYPE_KEY_ARCHITYPE: 'architype',
    TYPE_KEY_JSON_SAVE_HANDLER: 'json_save_handler',
    TYPE_KEY_JSON_LOAD_HANDLER: 'json_load_handler',
}

# MCP metadata — accepted on any type regardless of package overrides
_MCP_ATTR_MAP = {
    TYPE_KEY_MCP_SCHEMA: 'mcp_schema',
    TYPE_KEY_MCP_HINT: 'mcp_hint',
}

# Mapping from type config keys to attribute names for render overrides. Physics
# command handlers ride the same render-overrides gate: they are declared in the
# render override headers and registered in the same package_render_types_builder.
_RENDER_TYPE_ATTR_MAP = {
    TYPE_KEY_RENDER_CMD_BUILDER: 'render_cmd_builder',
    TYPE_KEY_RENDER_CMD_DESTROYER: 'render_cmd_destroyer',
    TYPE_KEY_RENDER_CMD_TRANSFORM: 'render_cmd_transform',
    TYPE_KEY_PHYSICS_CMD_BUILDER: 'physics_cmd_builder',
    TYPE_KEY_PHYSICS_CMD_DESTROYER: 'physics_cmd_destroyer',
    TYPE_KEY_PHYSICS_CMD_TRANSFORM: 'physics_cmd_transform',
}


def extract_type_config(type_obj: arapi.types.kryga_type, tokens: List[str],
                        context: arapi.types.file_context) -> None:
  """Extract and apply type configuration from tokens.

    Args:
        type_obj: The type object to configure
        tokens: List of key=value token strings
        context: File context with override flags
    """
  for token in tokens:
    pairs = token.strip().split("=", 1)    # Split only on first '='

    if len(pairs) != 2:
      continue

    key = arapi.utils.extstrip(pairs[0])
    if key == TYPE_KEY_MCP_HINT:
      value = pairs[1].strip().strip('"')
    else:
      value = arapi.utils.extstrip(pairs[1])

    matched = False

    # MCP metadata — always accepted
    if key in _MCP_ATTR_MAP:
      setattr(type_obj, _MCP_ATTR_MAP[key], value)
      matched = True

    # Model type overrides
    if context.model_has_types_overrides and key in _MODEL_TYPE_ATTR_MAP:
      setattr(type_obj, _MODEL_TYPE_ATTR_MAP[key], value)
      matched = True

    # External type configuration
    if type_obj.kind == arapi.types.kryga_type_kind.EXTERNAL:
      if key == TYPE_KEY_BUILT_IN:
        type_obj.built_in = True
        matched = True

    # Render type overrides
    if context.render_has_types_overrides and type_obj.kind == arapi.types.kryga_type_kind.CLASS:
      if key in _RENDER_TYPE_ATTR_MAP:
        setattr(type_obj, _RENDER_TYPE_ATTR_MAP[key], value)
        matched = True

    if not matched:
      raise ParserError(
          f"Unknown type metadata key '{key}' on type '{type_obj.name}'")


def _extract_class_name_from_line(line: str, line_number: int) -> Tuple[str, Optional[str]]:
  """Extract class name and optional parent name from a class declaration line.

    Args:
        line: Class declaration line
        line_number: Line number for error reporting (1-based)

    Returns:
        Tuple of (class_name, parent_name or None)
    """
  # Normalize the line: replace separators with spaces
  normalized = line.replace(" : ", " ").replace("\n", " ").replace(",", " ").split()

  # Filter out C++ keywords
  tokens = [t for t in normalized if t not in CXX_KEYWORDS]

  if not tokens:
    raise ParserError(f"Could not extract class name at line {line_number}: {line}")

  class_name = tokens[0].split('::')[-1]
  parent_name = tokens[1] if len(tokens) > 1 else None

  return class_name, parent_name


def _build_full_type_name(module_name: str, type_name: str, root_namespace: Optional[str]) -> str:
  """Build full type name with namespace.
    
    Args:
        module_name: Module name
        type_name: Type name
        root_namespace: Optional root namespace
        
    Returns:
        Full type name with namespace prefix
    """
  prefix = ''
  if root_namespace:
    if root_namespace.endswith('::' + module_name) or root_namespace == module_name:
      return f'{root_namespace}::{type_name}'
    prefix = f'{root_namespace}::'

  return f'{prefix}{module_name}::{type_name}'


def _parse_class(lines: List[str], index: int, lines_count: int, module_name: str,
                 context: arapi.types.file_context) -> Tuple[int, arapi.types.kryga_type]:
  """Parse an KRG_ar_class declaration.
    
    Args:
        lines: List of source lines
        index: Current line index
        lines_count: Total number of lines
        module_name: Module name
        context: File context
        
    Returns:
        Tuple of (new_index, kryga_type object)
    """
  class_type = arapi.types.kryga_type(arapi.types.kryga_type_kind.CLASS)
  class_type.has_namespace = True

  index, tokens = parse_attributes(MACRO_CLASS, lines, index, lines_count)
  index += 1

  extract_type_config(class_type, tokens, context)

  if index >= lines_count:
    raise ParserError(f"Unexpected end of file after KRG_ar_class at line {index + 1}")

  class_name, parent_name = _extract_class_name_from_line(lines[index], index + 1)
  class_type.name = class_name
  class_type.full_name = _build_full_type_name(module_name, class_name, context.root_namespace)

  if parent_name:
    class_type.parent_name = parent_name

  return index, class_type


def _parse_struct(lines: List[str], index: int, lines_count: int, module_name: str,
                  context: arapi.types.file_context) -> Tuple[int, arapi.types.kryga_type]:
  """Parse an KRG_ar_struct declaration.
    
    Args:
        lines: List of source lines
        index: Current line index
        lines_count: Total number of lines
        module_name: Module name
        context: File context
        
    Returns:
        Tuple of (new_index, kryga_type object)
    """
  struct_type = arapi.types.kryga_type(arapi.types.kryga_type_kind.STRUCT)
  struct_type.has_namespace = True

  index, tokens = parse_attributes(MACRO_STRUCT, lines, index, lines_count)
  index += 1

  extract_type_config(struct_type, tokens, context)

  if index >= lines_count:
    raise ParserError(f"Unexpected end of file after KRG_ar_struct at line {index + 1}")

  struct_name, _ = _extract_class_name_from_line(lines[index], index + 1)
  struct_type.name = struct_name
  struct_type.full_name = _build_full_type_name(module_name, struct_name, context.root_namespace)

  return index, struct_type


def _parse_construct_params(lines: List[str], index: int,
                            lines_count: int) -> Tuple[int, List[arapi.types.kryga_cparam]]:
  """Parse KRG_gen_construct_params { field; field; ... }; block."""
  result = []
  line = lines[index].strip()

  # Empty: KRG_gen_construct_params{};
  if "{}" in line or "{ }" in line:
    return index, result

  # Find opening brace
  while index < lines_count and "{" not in lines[index]:
    index += 1

  index += 1  # skip the line with {

  # Parse fields until closing brace
  while index < lines_count:
    line = lines[index].strip()
    if line.startswith("}"):
      break

    # Skip empty lines and comments
    if not line or line.startswith("//"):
      index += 1
      continue

    # Remove trailing ; and default value
    field = line.rstrip(";").strip()
    if "=" in field:
      field = field[:field.index("=")].strip()

    tokens = field.split()
    if len(tokens) < 2:
      index += 1
      continue

    name = tokens[-1]
    type_str = " ".join(tokens[:-1])

    is_pointer = name.endswith("*") or type_str.endswith("*")
    if name.endswith("*"):
      name = name[:-1]
    type_str = type_str.rstrip("*").rstrip()

    is_optional = type_str.startswith("std::optional<")
    if is_optional:
      type_str = type_str[len("std::optional<"):-1].strip()

    result.append(arapi.types.kryga_cparam(type_str, name, is_pointer, is_optional))
    index += 1

  return index, result


def _parse_function_metadata(func: arapi.types.kryga_function, metadata_tokens: List[str]) -> None:
  """Parse and apply function metadata tokens."""
  for token in metadata_tokens:
    token = token.strip()
    if token.startswith('"') and token.endswith('"'):
      token = token[1:-1]

    pairs = token.split("=", 1)
    if len(pairs) != 2:
      continue

    key = arapi.utils.extstrip(pairs[0])
    if key == PROP_KEY_MCP_HINT:
      value = pairs[1].strip().strip('"').strip("'")
    else:
      value = arapi.utils.extstrip(pairs[1])

    if key == PROP_KEY_CATEGORY:
      func.category = value
    elif key == PROP_KEY_MCP_HINT:
      func.mcp_hint = value


def _parse_function(lines: List[str], index: int,
                    lines_count: int) -> Tuple[int, arapi.types.kryga_function]:
  """Parse a KRG_ar_function declaration including metadata and C++ signature.

    Args:
        lines: List of source lines
        index: Current line index (pointing to line with KRG_ar_function)
        lines_count: Total number of lines

    Returns:
        Tuple of (new_index, kryga_function object)
    """
  function = arapi.types.kryga_function()
  macro_text = ""

  macro_text += lines[index] + " "
  while index <= lines_count - 1 and lines[index].find(")") == -1:
    index += 1
    if index < lines_count:
      macro_text += lines[index].strip() + " "

  # Extract metadata from macro args: KRG_ar_function("category=world", ...)
  macro_text = macro_text.strip()
  paren_start = macro_text.find("(")
  paren_end = macro_text.rfind(")")
  if paren_start != -1 and paren_end != -1:
    metadata_str = macro_text[paren_start + 1:paren_end].strip()
    if metadata_str:
      metadata_tokens = metadata_str.split(",")
      _parse_function_metadata(function, metadata_tokens)

  index += 1
  if index >= lines_count:
    raise ParserError(f"Unexpected end of file after KRG_ar_function at line {index + 1}")

  # Collect lines until we find the function signature's closing ')'
  signature = ""
  signature += lines[index] + " "
  while index <= lines_count - 1 and lines[index].find(")") == -1:
    index += 1
    if index < lines_count:
      signature += lines[index].strip() + " "

  signature = signature.strip().replace("\n", " ")

  # Parse: [return_type] name(params) [const] { ... }
  paren_open = signature.find("(")
  paren_close = signature.find(")", paren_open) if paren_open != -1 else -1

  if paren_open == -1:
    raise ParserError(f"Could not find '(' in function signature at line {index + 1}: {signature}")

  before_paren = signature[:paren_open].strip()
  tokens = before_paren.split()
  if len(tokens) >= 2:
    function.return_type = " ".join(tokens[:-1])
    function.name = tokens[-1]
  elif len(tokens) == 1:
    function.name = tokens[0]
  else:
    raise ParserError(f"Could not extract function name at line {index + 1}: {signature}")

  # Parse params
  if paren_close != -1:
    params_str = signature[paren_open + 1:paren_close].strip()
    after_close = signature[paren_close + 1:].strip()
    function.is_const = "const" in after_close

    if params_str and params_str != "void":
      for param_str in params_str.split(","):
        param_str = param_str.strip()
        if not param_str:
          continue
        param_tokens = param_str.split()
        if len(param_tokens) >= 2:
          p_type = " ".join(param_tokens[:-1])
          p_name = param_tokens[-1]
        else:
          p_type = param_tokens[0]
          p_name = ""
        function.params.append(arapi.types.kryga_function_param(p_type, p_name))

  return index, function


def _parse_property_metadata(prop: arapi.types.kryga_property, metadata_tokens: List[str]) -> None:
  """Parse and apply property metadata tokens.
    
    Args:
        prop: Property object to configure
        metadata_tokens: List of quoted key=value strings
        
    Raises:
        InvalidPropertyError: If property metadata is invalid
    """
  for token in metadata_tokens:
    token = token.strip()

    # Remove surrounding quotes if present
    if token.startswith('"') and token.endswith('"'):
      token = token[1:-1]

    pairs = token.split("=", 1)

    if len(pairs) != 2:
      raise InvalidPropertyError(
          f"Invalid property metadata format: '{token}'. Expected 'key=value'.")

    key = arapi.utils.extstrip(pairs[0])
    if key == PROP_KEY_MCP_HINT:
      value = pairs[1].strip().strip('"')
    else:
      value = arapi.utils.extstrip(pairs[1])

    # Map keys to property attributes
    if key == PROP_KEY_CATEGORY:
      prop.category = value
    elif key == PROP_KEY_SERIALIZABLE:
      if value not in VALID_BOOL_VALUES:
        raise InvalidPropertyError(f"serializable must be 'true' or 'false', got '{value}'")
      prop.serializable = value
    elif key == PROP_KEY_PROPERTY_SAVE_HANDLER:
      prop.property_save_handler = value
    elif key == PROP_KEY_PROPERTY_LOAD_HANDLER:
      prop.property_load_handler = value
    elif key == PROP_KEY_PROPERTY_COMPARE_HANDLER:
      prop.property_compare_handler = value
    elif key == PROP_KEY_PROPERTY_COPY_HANDLER:
      prop.property_copy_handler = value
    elif key == PROP_KEY_PROPERTY_SNAPSHOT_HANDLER:
      prop.property_snapshot_handler = value
    elif key == PROP_KEY_PROPERTY_INSTANTIATE_HANDLER:
      prop.property_instantiate_handler = value
    elif key == PROP_KEY_INSTANTIATE_MODE:
      if value not in ("share", "instantiate"):
        raise ValueError(f"Invalid instantiate mode '{value}' on property '{prop.name}'. Must be share or instantiate.")
      prop.instantiate_mode = value
    elif key == PROP_KEY_ACCESS:
      if value not in VALID_ACCESS_VALUES:
        raise InvalidPropertyError(
            f"access must be one of {sorted(VALID_ACCESS_VALUES)}, got '{value}'")
      prop.access = value
    elif key == PROP_KEY_DEFAULT:
      if value not in VALID_BOOL_VALUES:
        raise InvalidPropertyError(f"default must be 'true' or 'false', got '{value}'")
      prop.has_default = value
    elif key == PROP_KEY_GPU_DATA:
      prop.gpu_data = value
    elif key == PROP_KEY_GPU_TEXTURE_SLOT:
      try:
        prop.gpu_texture_slot = int(value)
      except ValueError:
        raise InvalidPropertyError(f"gpu_texture_slot must be an integer, got '{value}'")
    elif key == PROP_KEY_COPYABLE:
      if value not in VALID_YES_NO_VALUES:
        raise InvalidPropertyError(f"copyable must be 'yes' or 'no', got '{value}'")
      prop.copyable = value
    elif key == PROP_KEY_UPDATABLE:
      if value not in VALID_YES_NO_VALUES:
        raise InvalidPropertyError(f"updatable must be 'yes' or 'no', got '{value}'")
      prop.updatable = value
    elif key == PROP_KEY_REF:
      if value not in VALID_BOOL_VALUES:
        raise InvalidPropertyError(f"ref must be 'true' or 'false', got '{value}'")
      prop.ref = value
    elif key == PROP_KEY_INVALIDATES:
      _parse_invalidates(prop, value)
    elif key == PROP_KEY_CHECK:
      _parse_check(prop, value)
    elif key == PROP_KEY_HINT:
      _parse_hint(prop, value)
    elif key == PROP_KEY_MCP_HINT:
      prop.mcp_hint = value
    else:
      raise InvalidPropertyError(f"Unsupported property key: '{key}'")


def _parse_invalidates(prop: arapi.types.kryga_property, value: str) -> None:
  """Parse invalidates property metadata.
    
    Args:
        prop: Property object
        value: Comma-separated list of invalidation types
    """
  tokens = value.split(",")
  for token in tokens:
    token = arapi.utils.extstrip(token)
    if token == INVALIDATES_RENDER:
      prop.invalidates_render = True
    elif token == INVALIDATES_TRANSFORM:
      prop.invalidates_transform = True


def _parse_check(prop: arapi.types.kryga_property, value: str) -> None:
  """Parse check property metadata.
    
    Args:
        prop: Property object
        value: Comma-separated list of check types
    """
  tokens = value.split(",")
  for token in tokens:
    token = arapi.utils.extstrip(token)
    if token == CHECK_NOT_SAME:
      prop.check_not_same = True


def _parse_hint(prop: arapi.types.kryga_property, value: str) -> None:
  """Parse hint property metadata.
    
    Args:
        prop: Property object
        value: Comma-separated list of hint values
    """
  tokens = value.split(",")
  hint_parts = []
  for token in tokens:
    token = arapi.utils.extstrip(token)
    hint_parts.append(f'"{token}"')

  prop.hint = ",".join(hint_parts)


def _parse_property(lines: List[str], index: int, lines_count: int,
                    class_name: str) -> Tuple[int, arapi.types.kryga_property]:
  """Parse an KRG_ar_property declaration.
    
    Args:
        lines: List of source lines
        index: Current line index
        lines_count: Total number of lines
        class_name: Name of the owning class
        
    Returns:
        Tuple of (new_index, kryga_property object)
        
    Raises:
        InvalidPropertyError: If property parsing fails
    """
  prop = arapi.types.kryga_property()

  index, metadata_tokens = parse_attributes(MACRO_PROPERTY, lines, index, lines_count)
  index += 1

  if index >= lines_count:
    raise ParserError(f"Unexpected end of file after KRG_ar_property at line {index + 1}")

  # Parse property declaration line
  property_line = lines[index].strip()
  if property_line.endswith(';'):
    property_line = property_line[:-1]

  property_tokens = property_line.split()

  if len(property_tokens) < 2:
    raise InvalidPropertyError(f"Invalid property declaration: {property_line}")

  prop.type = property_tokens[0]
  prop.name = property_tokens[1]
  prop.name_cut = property_tokens[1][2:] if len(property_tokens[1]) > 2 else ""
  prop.owner = class_name

  # Check for default value if default metadata is present
  has_default_metadata = any(token.strip().startswith(f'"{PROP_KEY_DEFAULT}=')
                             or token.strip().startswith(f'{PROP_KEY_DEFAULT}=')
                             for token in metadata_tokens)

  if has_default_metadata:
    if len(property_tokens) < 3 or property_tokens[2] != "=":
      raise InvalidPropertyError("Property has 'default' metadata but no default value provided")

  _parse_property_metadata(prop, metadata_tokens)

  return index, prop


def _parse_constructor(lines: List[str], index: int,
                       lines_count: int) -> Tuple[int, arapi.types.kryga_ctor]:
  """Parse an KRG_ar_ctor declaration.
    
    Args:
        lines: List of source lines
        index: Current line index
        lines_count: Total number of lines
        
    Returns:
        Tuple of (new_index, kryga_ctor object)
    """
  ctor = arapi.types.kryga_ctor()

  # Skip constructor header (first set of parentheses)
  while index < lines_count and ")" not in lines[index]:
    index += 1

  index += 1

  if index >= lines_count:
    raise ParserError(f"Unexpected end of file after KRG_ar_ctor header at line {index + 1}")

  # Parse constructor body (second set of parentheses)
  ctor_body = lines[index]
  index += 1

  while index < lines_count and ")" not in lines[index]:
    ctor_body += lines[index].strip() + " "
    index += 1

  ctor.name = ctor_body.strip().replace("\n", " ")

  return index, ctor


def _parse_external_type(lines: List[str], index: int, lines_count: int,
                         context: arapi.types.file_context) -> Tuple[int, arapi.types.kryga_type]:
  """Parse an KRG_ar_external_type declaration.
    
    Args:
        lines: List of source lines
        index: Current line index
        lines_count: Total number of lines
        context: File context
        
    Returns:
        Tuple of (new_index, kryga_type object)
    """
  index, tokens = parse_attributes(MACRO_EXTERNAL_TYPE, lines, index, lines_count)

  external_type = arapi.types.kryga_type(arapi.types.kryga_type_kind.EXTERNAL)
  extract_type_config(external_type, tokens, context)

  index += 1

  if index >= lines_count:
    raise ParserError(f"Unexpected end of file after KRG_ar_external_type at line {index + 1}")

  # Parse external type declaration
  external_line = lines[index]
  normalized = external_line.replace(";", " ").replace("\n", " ").replace(",", " ").replace(
      "(", " ").replace(")", " ").split()

  tokens = [t for t in normalized if t not in CXX_KEYWORDS]

  if not tokens:
    raise ParserError(f"Could not extract external type name at line {index + 1}: {external_line}")

  external_type.full_name = tokens[0].removeprefix('::')
  external_type.name = external_type.full_name.split('::')[-1]

  return index, external_type


def _parse_package(lines: List[str], index: int, lines_count: int,
                   context: arapi.types.file_context) -> int:
  """Parse an KRG_ar_package declaration.
    
    Args:
        lines: List of source lines
        index: Current line index
        lines_count: Total number of lines
        context: File context to update
        
    Returns:
        New line index
    """
  index, tokens = parse_attributes(MACRO_PACKAGE, lines, index, lines_count)
  index += 1

  for token in tokens:
    pairs = token.strip().split("=", 1)

    if len(pairs) == 2:
      key = arapi.utils.extstrip(pairs[0])
      value = arapi.utils.extstrip(pairs[1])

      if key == PKG_KEY_MODEL_TYPES_OVERRIDES:
        context.model_has_types_overrides = value == "true"
      elif key == PKG_KEY_MODEL_PROPERTIES_OVERRIDES:
        context.model_has_properties_overrides = value == "true"
      elif key == PKG_KEY_RENDER_OVERRIDES:
        context.render_has_types_overrides = value == "true"
      elif key == PKG_KEY_RENDER_RESOURCES:
        context.render_has_custom_resources = value == "true"
      elif key == PKG_KEY_EDITOR_OVERRIDES:
        context.editor_has_overrides = value == "true"
      elif key == PKG_KEY_DEPENDENCIES:
        context.dependencies = value.split(":")

  return index


def _add_include(context: arapi.types.file_context, file_rel_path: str) -> None:
  """Add include statement to context.

    Args:
        context: File context
        file_rel_path: Relative file path
    """
  if file_rel_path.startswith(INCLUDE_PREFIX):
    include_path = file_rel_path[INCLUDE_PREFIX_LEN:]
  else:
    include_path = file_rel_path

  context.includes.add(f'#include "{include_path}"')


def _handle_model_overrides(context: arapi.types.file_context, file_rel_path: str) -> None:
  """Handle KRG_ar_model_overrides macro.

    Args:
        context: File context to update
        file_rel_path: Relative file path
    """
  include_path = file_rel_path.removeprefix(INCLUDE_PREFIX)
  context.model_overrides.append(include_path)
  context.includes.add(f'#include "{include_path}"')


def _handle_render_overrides(context: arapi.types.file_context, file_rel_path: str) -> None:
  """Handle KRG_ar_render_overrides macro.

    Args:
        context: File context to update
        file_rel_path: Relative file path
    """
  include_path = file_rel_path.removeprefix(INCLUDE_PREFIX)
  context.render_overrides.append(include_path)
  context.includes.add(f'#include "{include_path}"')


def _handle_editor_overrides(context: arapi.types.file_context, file_rel_path: str) -> None:
  """Handle KRG_ar_editor_overrides macro.

    Args:
        context: File context to update
        file_rel_path: Relative file path
    """
  include_path = file_rel_path.removeprefix(INCLUDE_PREFIX)
  context.editor_overrides.append(include_path)
  context.includes.add(f'#include "{include_path}"')


@dataclass
class _ParserState:
  """Internal state for file parsing."""
  lines: List[str]
  lines_count: int
  module_name: str
  class_name: str
  context: arapi.types.file_context
  original_file_rel_path: str
  current_class: Optional[arapi.types.kryga_type] = None
  current_struct: Optional[arapi.types.kryga_type] = None


def _read_file(file_path: str) -> Tuple[List[str], int]:
  """Read file and return lines with count.

    Args:
        file_path: Path to file to read

    Returns:
        Tuple of (lines, line_count)
    """
  with open(file_path, "r", encoding="utf-8") as f:
    lines = f.readlines()
  return lines, len(lines)


def _finalize_parsing(state: _ParserState) -> None:
  """Finalize parsing by adding types and includes to context.

    Args:
        state: Parser state with parsed types
    """
  if state.current_class:
    state.context.types.append(state.current_class)

  if state.current_struct:
    state.context.types.append(state.current_struct)

  _add_include(state.context, state.original_file_rel_path)


def parse_file(original_file_full_path: str, original_file_rel_path: str, module_name: str,
               context: arapi.types.file_context) -> None:
  """Parse a C++ header file and extract KRYGA reflection metadata.

    Args:
        original_file_full_path: Full path to the file
        original_file_rel_path: Relative path to the file
        module_name: Name of the module
        context: File context to populate

    Raises:
        ParserError: If parsing fails
    """
  #arapi.utils.eprint(f"processing : {original_file_full_path} ...")

  lines, lines_count = _read_file(original_file_full_path)
  class_name = Path(original_file_full_path).stem    # Filename without extension

  state = _ParserState(
      lines=lines,
      lines_count=lines_count,
      module_name=module_name,
      class_name=class_name,
      context=context,
      original_file_rel_path=original_file_rel_path,
  )

  i = 0
  while i < lines_count:
    line = lines[i].strip()

    # Handle model overrides
    if line.startswith(MACRO_MODEL_OVERRIDES):
      _handle_model_overrides(context, original_file_rel_path)
      i += 1
      continue

    # Handle render overrides
    if line.startswith(MACRO_RENDER_OVERRIDES):
      _handle_render_overrides(context, original_file_rel_path)
      i += 1
      continue

    # Handle editor overrides
    if line.startswith(MACRO_EDITOR_OVERRIDES):
      _handle_editor_overrides(context, original_file_rel_path)
      i += 1
      continue

    # Handle class declaration
    if line.startswith(MACRO_CLASS):
      if state.current_class:
        raise ParserError(
            f"Nested class declaration not allowed at line {i + 1}. "
            f"Already parsing '{state.current_class.name}'")
      i, state.current_class = _parse_class(lines, i, lines_count, module_name, context)
      state.current_class.source_file = state.original_file_rel_path
      i += 1
      continue

    # Handle struct declaration
    if line.startswith(MACRO_STRUCT):
      if state.current_struct:
        raise ParserError(
            f"Nested struct declaration not allowed at line {i + 1}. "
            f"Already parsing '{state.current_struct.name}'")
      i, state.current_struct = _parse_struct(lines, i, lines_count, module_name, context)
      state.current_struct.source_file = state.original_file_rel_path
      i += 1
      continue

    # Handle function declaration
    if line.startswith(MACRO_FUNCTION):
      if not state.current_class and not state.current_struct:
        raise ParserError(f"KRG_ar_function found outside of class or struct at line {i + 1}")

      i, function = _parse_function(lines, i, lines_count)

      if state.current_class:
        state.current_class.functions.append(function)
      if state.current_struct:
        state.current_struct.functions.append(function)

      i += 1
      continue

    # Handle construct_params block
    if line.startswith(MACRO_CONSTRUCT_PARAMS):
      if state.current_class:
        i, cparams = _parse_construct_params(lines, i, lines_count)
        state.current_class.construct_params = cparams
      i += 1
      continue

    # Handle property declaration
    if line.startswith(MACRO_PROPERTY):
      if not state.current_class:
        raise ParserError(f"KRG_ar_property found outside of class at line {i + 1}")

      i, prop = _parse_property(lines, i, lines_count, class_name)
      state.current_class.properties.append(prop)
      i += 1
      continue

    # Handle constructor declaration
    if line.startswith(MACRO_CTOR):
      if not state.current_struct:
        raise ParserError(f"KRG_ar_ctor found outside of struct at line {i + 1}")

      i, ctor = _parse_constructor(lines, i, lines_count)
      state.current_struct.ctros.append(ctor)
      i += 1
      continue

    # Handle external type declaration
    if line.startswith(MACRO_EXTERNAL_TYPE):
      i, external_type = _parse_external_type(lines, i, lines_count, context)
      context.types.append(external_type)
      i += 1
      continue

    # Handle package declaration
    if line.startswith(MACRO_PACKAGE):
      i = _parse_package(lines, i, lines_count, context)
      continue

    i += 1

  _finalize_parsing(state)
