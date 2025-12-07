"""Parser module for AGEA reflection system.

This module parses C++ header files annotated with AGEA macros to extract
type information, properties, functions, and other metadata.
"""
import os
from typing import List, Tuple, Optional

import arapi.types
import arapi.utils

# Constants
INCLUDE_PREFIX = "include/"
INCLUDE_PREFIX_LEN = len(INCLUDE_PREFIX)

# AGEA macro markers
MACRO_MODEL_OVERRIDES = "AGEA_ar_model_overrides()"
MACRO_RENDER_OVERRIDES = "AGEA_ar_render_overrides()"
MACRO_CLASS = "AGEA_ar_class"
MACRO_STRUCT = "AGEA_ar_struct"
MACRO_FUNCTION = "AGEA_ar_function"
MACRO_PROPERTY = "AGEA_ar_property"
MACRO_CTOR = "AGEA_ar_ctor"
MACRO_EXTERNAL_TYPE = "AGEA_ar_external_type("
MACRO_PACKAGE = "AGEA_ar_package"

# Property metadata keys
PROP_KEY_CATEGORY = "category"
PROP_KEY_SERIALIZABLE = "serializable"
PROP_KEY_ACCESS = "access"
PROP_KEY_DEFAULT = "default"
PROP_KEY_GPU_DATA = "gpu_data"
PROP_KEY_COPYABLE = "copyable"
PROP_KEY_UPDATABLE = "updatable"
PROP_KEY_REF = "ref"
PROP_KEY_INVALIDATES = "invalidates"
PROP_KEY_CHECK = "check"
PROP_KEY_HINT = "hint"
PROP_KEY_PROPERTY_SER_HANDLER = "property_ser_handler"
PROP_KEY_PROPERTY_DES_HANDLER = "property_des_handler"
PROP_KEY_PROPERTY_LOAD_DERIVE_HANDLER = "property_load_derive_handler"
PROP_KEY_PROPERTY_COMPARE_HANDLER = "property_compare_handler"
PROP_KEY_PROPERTY_COPY_HANDLER = "property_copy_handler"
PROP_KEY_PROPERTY_INSTANTIATE_HANDLER = "property_instantiate_handler"

# Type config keys
TYPE_KEY_COPY_HANDLER = "copy_handler"
TYPE_KEY_INSTANTIATE_HANDLER = "instantiate_handler"
TYPE_KEY_COMPARE_HANDLER = "compare_handler"
TYPE_KEY_SERIALIZE_HANDLER = "serialize_handler"
TYPE_KEY_DESERIALIZE_HANDLER = "deserialize_handler"
TYPE_KEY_LOAD_DERIVE_HANDLER = "load_derive_handler"
TYPE_KEY_TO_STRING_HANDLER = "to_string_handler"
TYPE_KEY_ARCHITYPE = "architype"
TYPE_KEY_BUILT_IN = "built_in"
TYPE_KEY_RENDER_CONSTRUCTOR = "render_constructor"
TYPE_KEY_RENDER_DESTRUCTOR = "render_destructor"

# Package config keys
PKG_KEY_MODEL_TYPES_OVERRIDES = "model.has_types_overrides"
PKG_KEY_MODEL_PROPERTIES_OVERRIDES = "model.has_properties_overrides"
PKG_KEY_RENDER_OVERRIDES = "render.has_overrides"
PKG_KEY_RENDER_RESOURCES = "render.has_resources"
PKG_KEY_DEPENDENCIES = "dependancies"

# Invalidates values
INVALIDATES_RENDER = "render"
INVALIDATES_TRANSFORM = "transform"

# Check values
CHECK_NOT_SAME = "not_same"

# C++ keywords to filter
CXX_KEYWORDS = {"class", "struct", "public", "private", "AGEA_ar_external_define", ""}


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
        SystemExit: If value is not 'true' or 'false' (for backward compatibility)
    """
  if value == 'true':
    return True
  elif value == 'false':
    return False
  else:
    arapi.utils.eprint('UNSUPPORTED!')
    exit(1)


def extract_type_config(type_obj: arapi.types.agea_type, tokens: List[str],
                        context: arapi.types.file_context) -> None:
  """Extract and apply type configuration from tokens.
    
    Args:
        type_obj: The type object to configure
        tokens: List of key=value token strings
        context: File context with override flags
    """
  for token in tokens:
    pairs = token.strip().split("=", 1)    # Split only on first '='

    if len(pairs) == 1:
      key = arapi.utils.extstrip(pairs[0])
      continue

    if len(pairs) == 2:
      key = arapi.utils.extstrip(pairs[0])
      value = arapi.utils.extstrip(pairs[1])

      # Model type overrides
      if context.model_has_types_overrides:
        if key == TYPE_KEY_COPY_HANDLER:
          type_obj.copy_handler = value
        elif key == TYPE_KEY_INSTANTIATE_HANDLER:
          type_obj.instantiate_handler = value
        elif key == TYPE_KEY_COMPARE_HANDLER:
          type_obj.compare_handler = value
        elif key == TYPE_KEY_SERIALIZE_HANDLER:
          type_obj.serialize_handler = value
        elif key == TYPE_KEY_DESERIALIZE_HANDLER:
          type_obj.deserialize_handler = value
        elif key == TYPE_KEY_LOAD_DERIVE_HANDLER:
          type_obj.load_derive_handler = value
        elif key == TYPE_KEY_TO_STRING_HANDLER:
          type_obj.to_string_handle = value
        elif key == TYPE_KEY_ARCHITYPE:
          type_obj.architype = value

      # External type configuration
      if type_obj.kind == arapi.types.agea_type_kind.EXTERNAL:
        if key == TYPE_KEY_BUILT_IN:
          type_obj.built_in = True

      # Render type overrides
      if context.render_has_types_overrides and type_obj.kind == arapi.types.agea_type_kind.CLASS:
        if key == TYPE_KEY_RENDER_CONSTRUCTOR:
          type_obj.render_constructor = value
        elif key == TYPE_KEY_RENDER_DESTRUCTOR:
          type_obj.render_destructor = value


def _extract_class_name_from_line(line: str) -> Tuple[str, Optional[str]]:
  """Extract class name and optional parent name from a class declaration line.
    
    Args:
        line: Class declaration line
        
    Returns:
        Tuple of (class_name, parent_name or None)
    """
  # Normalize the line: replace separators with spaces
  normalized = line.replace(" : ", " ").replace("\n", " ").replace(",", " ").split()

  # Filter out C++ keywords
  tokens = [t for t in normalized if t not in CXX_KEYWORDS]

  if not tokens:
    raise ParserError(f"Could not extract class name from line: {line}")

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
    prefix = f'{root_namespace}::'

  return f'{prefix}{module_name}::{type_name}'


def _parse_class(lines: List[str], index: int, lines_count: int, module_name: str,
                 context: arapi.types.file_context) -> Tuple[int, arapi.types.agea_type]:
  """Parse an AGEA_ar_class declaration.
    
    Args:
        lines: List of source lines
        index: Current line index
        lines_count: Total number of lines
        module_name: Module name
        context: File context
        
    Returns:
        Tuple of (new_index, agea_type object)
    """
  class_type = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
  class_type.has_namespace = True

  index, tokens = parse_attributes(MACRO_CLASS, lines, index, lines_count)
  index += 1

  extract_type_config(class_type, tokens, context)

  if index >= lines_count:
    raise ParserError("Unexpected end of file after AGEA_ar_class")

  class_name, parent_name = _extract_class_name_from_line(lines[index])
  class_type.name = class_name
  class_type.full_name = _build_full_type_name(module_name, class_name, context.root_namespace)

  if parent_name:
    class_type.parent_name = parent_name

  return index, class_type


def _parse_struct(lines: List[str], index: int, lines_count: int, module_name: str,
                  context: arapi.types.file_context) -> Tuple[int, arapi.types.agea_type]:
  """Parse an AGEA_ar_struct declaration.
    
    Args:
        lines: List of source lines
        index: Current line index
        lines_count: Total number of lines
        module_name: Module name
        context: File context
        
    Returns:
        Tuple of (new_index, agea_type object)
    """
  struct_type = arapi.types.agea_type(arapi.types.agea_type_kind.STRUCT)
  struct_type.has_namespace = True

  index, tokens = parse_attributes(MACRO_STRUCT, lines, index, lines_count)
  index += 1

  extract_type_config(struct_type, tokens, context)

  if index >= lines_count:
    raise ParserError("Unexpected end of file after AGEA_ar_struct")

  struct_name, _ = _extract_class_name_from_line(lines[index])
  struct_type.name = struct_name
  struct_type.full_name = _build_full_type_name(module_name, struct_name, context.root_namespace)

  return index, struct_type


def _parse_function(lines: List[str], index: int,
                    lines_count: int) -> Tuple[int, arapi.types.agea_function]:
  """Parse an AGEA_ar_function declaration.
    
    Args:
        lines: List of source lines
        index: Current line index (pointing to line with AGEA_ar_function)
        lines_count: Total number of lines
        
    Returns:
        Tuple of (new_index, agea_function object)
    """
  function = arapi.types.agea_function()
  function_header = ""
  function_body = ""

  # Start with current line (contains AGEA_ar_function)
  function_header += lines[index] + " "

  # Find closing parenthesis of function header (matching original logic)
  while index <= lines_count - 1 and lines[index].find(")") == -1:
    index += 1
    if index < lines_count:
      function_header += lines[index].strip() + " "

  # Move past the line with closing paren
  index += 1

  if index >= lines_count:
    raise ParserError("Unexpected end of file after AGEA_ar_function header")

  # Start function body with current line
  function_body += lines[index] + " "

  # Find closing parenthesis of function body (matching original logic)
  while index <= lines_count - 1 and lines[index].find(")") == -1:
    index += 1
    if index < lines_count:
      function_body += lines[index].strip() + " "

  # Extract function name
  function_body = function_body.strip().replace("\n", " ")
  tokens = function_body.split(" ")

  try:
    function_token = next(token for token in tokens if token.find("(") != -1)
    function.name = function_token[:function_token.find("(")]
  except StopIteration:
    raise ParserError(f"Could not extract function name from: {function_body}")

  return index, function


def _parse_property_metadata(prop: arapi.types.agea_property, metadata_tokens: List[str]) -> None:
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
    value = arapi.utils.extstrip(pairs[1])

    # Map keys to property attributes
    if key == PROP_KEY_CATEGORY:
      prop.category = value
    elif key == PROP_KEY_SERIALIZABLE:
      prop.serializable = value
    elif key == PROP_KEY_PROPERTY_SER_HANDLER:
      prop.property_ser_handler = value
    elif key == PROP_KEY_PROPERTY_DES_HANDLER:
      prop.property_des_handler = value
    elif key == PROP_KEY_PROPERTY_LOAD_DERIVE_HANDLER:
      prop.property_load_derive_handler = value
    elif key == PROP_KEY_PROPERTY_COMPARE_HANDLER:
      prop.property_compare_handler = value
    elif key == PROP_KEY_PROPERTY_COPY_HANDLER:
      prop.property_copy_handler = value
    elif key == PROP_KEY_PROPERTY_INSTANTIATE_HANDLER:
      prop.property_instantiate_handler = value
    elif key == PROP_KEY_ACCESS:
      prop.access = value
    elif key == PROP_KEY_DEFAULT:
      prop.has_default = value
    elif key == PROP_KEY_GPU_DATA:
      prop.gpu_data = value
    elif key == PROP_KEY_COPYABLE:
      prop.copyable = value
    elif key == PROP_KEY_UPDATABLE:
      prop.copyable = value    # Note: Bug preserved for backward compatibility
    elif key == PROP_KEY_REF:
      prop.ref = value
    elif key == PROP_KEY_INVALIDATES:
      _parse_invalidates(prop, value)
    elif key == PROP_KEY_CHECK:
      _parse_check(prop, value)
    elif key == PROP_KEY_HINT:
      _parse_hint(prop, value)
    else:
      raise InvalidPropertyError(f"Unsupported property key: '{key}'")


def _parse_invalidates(prop: arapi.types.agea_property, value: str) -> None:
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


def _parse_check(prop: arapi.types.agea_property, value: str) -> None:
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


def _parse_hint(prop: arapi.types.agea_property, value: str) -> None:
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
                    class_name: str) -> Tuple[int, arapi.types.agea_property]:
  """Parse an AGEA_ar_property declaration.
    
    Args:
        lines: List of source lines
        index: Current line index
        lines_count: Total number of lines
        class_name: Name of the owning class
        
    Returns:
        Tuple of (new_index, agea_property object)
        
    Raises:
        InvalidPropertyError: If property parsing fails
    """
  prop = arapi.types.agea_property()

  index, metadata_tokens = parse_attributes(MACRO_PROPERTY, lines, index, lines_count)
  index += 1

  if index >= lines_count:
    raise ParserError("Unexpected end of file after AGEA_ar_property")

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
                       lines_count: int) -> Tuple[int, arapi.types.agea_ctor]:
  """Parse an AGEA_ar_ctor declaration.
    
    Args:
        lines: List of source lines
        index: Current line index
        lines_count: Total number of lines
        
    Returns:
        Tuple of (new_index, agea_ctor object)
    """
  ctor = arapi.types.agea_ctor()

  # Skip constructor header (first set of parentheses)
  while index < lines_count and ")" not in lines[index]:
    index += 1

  index += 1

  if index >= lines_count:
    raise ParserError("Unexpected end of file after AGEA_ar_ctor header")

  # Parse constructor body (second set of parentheses)
  ctor_body = lines[index]
  index += 1

  while index < lines_count and ")" not in lines[index]:
    ctor_body += lines[index].strip() + " "
    index += 1

  ctor.name = ctor_body.strip().replace("\n", " ")

  return index, ctor


def _parse_external_type(lines: List[str], index: int, lines_count: int,
                         context: arapi.types.file_context) -> Tuple[int, arapi.types.agea_type]:
  """Parse an AGEA_ar_external_type declaration.
    
    Args:
        lines: List of source lines
        index: Current line index
        lines_count: Total number of lines
        context: File context
        
    Returns:
        Tuple of (new_index, agea_type object)
    """
  index, tokens = parse_attributes(MACRO_EXTERNAL_TYPE, lines, index, lines_count)

  external_type = arapi.types.agea_type(arapi.types.agea_type_kind.EXTERNAL)
  extract_type_config(external_type, tokens, context)

  index += 1

  if index >= lines_count:
    raise ParserError("Unexpected end of file after AGEA_ar_external_type")

  # Parse external type declaration
  external_line = lines[index]
  normalized = external_line.replace(";", " ").replace("\n", " ").replace(",", " ").replace(
      "(", " ").replace(")", " ").split()

  tokens = [t for t in normalized if t not in CXX_KEYWORDS]

  if not tokens:
    raise ParserError(f"Could not extract external type name from: {external_line}")

  external_type.full_name = tokens[0].removeprefix('::')
  external_type.name = external_type.full_name.split('::')[-1]

  return index, external_type


def _parse_package(lines: List[str], index: int, lines_count: int,
                   context: arapi.types.file_context) -> int:
  """Parse an AGEA_ar_package declaration.
    
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


def parse_file(original_file_full_path: str, original_file_rel_path: str, module_name: str,
               context: arapi.types.file_context) -> None:
  """Parse a C++ header file and extract AGEA reflection metadata.
    
    Args:
        original_file_full_path: Full path to the file
        original_file_rel_path: Relative path to the file
        module_name: Name of the module
        context: File context to populate
        
    Raises:
        ParserError: If parsing fails
    """
  arapi.utils.eprint(f"processing : {original_file_full_path} ...")

  class_name = os.path.basename(original_file_full_path)[:-2]    # Remove '.h' extension

  with open(original_file_full_path, "r", encoding="utf-8") as cfg:
    lines = cfg.readlines()
    lines_count = len(lines)

  current_class: Optional[arapi.types.agea_type] = None
  current_struct: Optional[arapi.types.agea_type] = None

  i = 0
  while i < lines_count:
    line = lines[i].strip()

    # Handle model overrides
    if line.startswith(MACRO_MODEL_OVERRIDES):
      include_path = original_file_rel_path.removeprefix(INCLUDE_PREFIX)
      context.model_overrides.append(include_path)
      context.includes.add(f'#include "{include_path}"')
      i += 1
      continue

    # Handle render overrides
    if line.startswith(MACRO_RENDER_OVERRIDES):
      include_path = original_file_rel_path.removeprefix(INCLUDE_PREFIX)
      context.render_overrides.append(include_path)
      context.includes.add(f'#include "{include_path}"')
      i += 1
      continue

    # Handle class declaration
    if line.startswith(MACRO_CLASS):
      i, current_class = _parse_class(lines, i, lines_count, module_name, context)
      i += 1
      continue

    # Handle struct declaration
    if line.startswith(MACRO_STRUCT):
      i, current_struct = _parse_struct(lines, i, lines_count, module_name, context)
      i += 1
      continue

    # Handle function declaration
    if line.startswith(MACRO_FUNCTION):
      if not current_class and not current_struct:
        raise ParserError("AGEA_ar_function found outside of class or struct")

      i, function = _parse_function(lines, i, lines_count)

      if current_class:
        current_class.functions.append(function)
      if current_struct:
        current_struct.functions.append(function)

      i += 1
      continue

    # Handle property declaration
    if line.startswith(MACRO_PROPERTY):
      if not current_class:
        raise ParserError("AGEA_ar_property found outside of class")

      i, prop = _parse_property(lines, i, lines_count, class_name)
      current_class.properties.append(prop)
      i += 1
      continue

    # Handle constructor declaration
    if line.startswith(MACRO_CTOR):
      if not current_struct:
        raise ParserError("AGEA_ar_ctor found outside of struct")

      i, ctor = _parse_constructor(lines, i, lines_count)
      current_struct.ctros.append(ctor)
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

  # Add final types to context
  if current_class:
    context.types.append(current_class)

  if current_struct:
    context.types.append(current_struct)

  # Add include statement
  _add_include(context, original_file_rel_path)
