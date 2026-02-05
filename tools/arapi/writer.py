"""Code generation module for KRYGA reflection system.

This module generates C++ header and source files for reflection metadata,
including class includes, package includes, type resolvers, Lua bindings,
and reflection implementations.
"""
import os
from typing import TextIO, Set, List, Optional, Tuple, Dict
from collections import OrderedDict, deque
from dataclasses import dataclass
import arapi.types
import arapi.utils


# GPU type mapping: model_type -> (gpu_type_macro, glsl_type, alignment, size)
GPU_TYPE_MAP: Dict[str, Tuple[str, str, int, int]] = {
    "float": ("std140_float", "float", 4, 4),
    "double": ("std140_float", "float", 4, 4),  # downcast to float for GPU
    "int": ("std140_int", "int", 4, 4),
    "int32_t": ("std140_int", "int", 4, 4),
    "uint32_t": ("std140_uint", "uint", 4, 4),
    "bool": ("std140_uint", "uint", 4, 4),  # bool as uint on GPU
    "::kryga::root::vec2": ("std140_vec2", "vec2", 8, 8),
    "::kryga::root::vec3": ("std140_vec3", "vec3", 16, 12),
    "::kryga::root::vec4": ("std140_vec4", "vec4", 16, 16),
    "::kryga::root::mat3": ("std140_mat3", "mat3", 16, 48),
    "::kryga::root::mat4": ("std140_mat4", "mat4", 16, 64),
    "::kryga::root::texture_slot": ("std140_uint", "uint", 4, 4),  # texture slot -> bindless index
}


@dataclass
class GpuField:
  """Represents a field in a GPU struct."""
  name: str
  model_type: str
  gpu_type: str
  glsl_type: str
  alignment: int
  size: int
  is_texture_slot: bool
  texture_slot_index: int
  owner_class: str  # original class that defined this property


def _get_gpu_type_info(model_type: str) -> Optional[Tuple[str, str, int, int]]:
  """Get GPU type info for a model type.

  Args:
      model_type: The C++ model type string

  Returns:
      Tuple of (gpu_type_macro, glsl_type, alignment, size) or None if not mappable
  """
  # Direct lookup
  if model_type in GPU_TYPE_MAP:
    return GPU_TYPE_MAP[model_type]

  # Try without leading ::
  clean_type = model_type.lstrip(':')
  for key, value in GPU_TYPE_MAP.items():
    if key.lstrip(':') == clean_type:
      return value

  return None


def _collect_gpu_fields_from_type(type_obj: arapi.types.kryga_type,
                                   context: arapi.types.file_context) -> List[GpuField]:
  """Collect GPU fields from a type and all its parents (flattened).

  Args:
      type_obj: The type to collect fields from
      context: File context with all types

  Returns:
      List of GpuField objects, sorted by alignment (largest first)
  """
  fields: List[GpuField] = []
  seen_names: Set[str] = set()

  # Walk the inheritance chain
  current = type_obj
  while current is not None:
    for prop in current.properties:
      # Skip if already seen (child overrides parent)
      if prop.name in seen_names:
        continue

      # Check if this is a GPU field
      is_gpu_data = prop.gpu_data != ""
      is_texture_slot = prop.gpu_texture_slot >= 0

      if not is_gpu_data and not is_texture_slot:
        continue

      # Get GPU type info
      if is_texture_slot:
        gpu_type = "std140_uint"
        glsl_type = "uint"
        alignment = 4
        size = 4
      else:
        type_info = _get_gpu_type_info(prop.type)
        if type_info is None:
          arapi.utils.eprint(f"Warning: Unknown GPU type mapping for '{prop.type}' in {prop.owner}::{prop.name}")
          continue
        gpu_type, glsl_type, alignment, size = type_info

      fields.append(GpuField(
          name=prop.name,
          model_type=prop.type,
          gpu_type=gpu_type,
          glsl_type=glsl_type,
          alignment=alignment,
          size=size,
          is_texture_slot=is_texture_slot,
          texture_slot_index=prop.gpu_texture_slot,
          owner_class=current.name,
      ))
      seen_names.add(prop.name)

    # Move to parent
    current = current.parent_type

  # Sort by alignment descending (largest first for optimal packing)
  fields.sort(key=lambda f: (-f.alignment, f.name))

  return fields


def _generate_pack_statement(field: GpuField, src_var: str, dst_var: str, owner_full_type: str) -> str:
  """Generate the pack statement for a field using offset-based access.

  Args:
      field: The GPU field
      src_var: Source variable name (e.g., "src")
      dst_var: Destination variable name (e.g., "dst")
      owner_full_type: Full type name of the owning class

  Returns:
      C++ statement(s) to pack the value
  """
  # Strip m_ prefix for GPU struct field names (matches GLSL convention)
  gpu_field_name = field.name[2:] if field.name.startswith("m_") else field.name

  if field.is_texture_slot:
    # Texture slot: get the slot via offset, then access txt pointer
    return f"""    {{
        const auto& slot = *reinterpret_cast<const ::kryga::root::texture_slot*>(blob + offsetof({owner_full_type}, {field.name}));
        if (slot.txt && slot.txt->get_texture_data())
            {dst_var}.{gpu_field_name} = slot.txt->get_texture_data()->get_bindless_index();
        else
            {dst_var}.{gpu_field_name} = UINT32_MAX;
    }}"""
  else:
    # Regular field: use memcpy with offset (source uses original name with m_, dest uses stripped name)
    return f"    std::memcpy(&{dst_var}.{gpu_field_name}, blob + offsetof({owner_full_type}, {field.name}), sizeof({dst_var}.{gpu_field_name}));"


def write_gpu_struct(type_obj: arapi.types.kryga_type,
                     context: arapi.types.file_context) -> bool:
  """Write GPU struct and pack function for a type.

  Args:
      type_obj: The type to generate GPU struct for
      context: File context

  Returns:
      True if GPU struct was generated, False if type has no GPU fields
  """
  if type_obj.kind != arapi.types.kryga_type_kind.CLASS:
    return False

  fields = _collect_gpu_fields_from_type(type_obj, context)

  if not fields:
    return False

  if not os.path.exists(context.gpu_types_dir):
    os.makedirs(context.gpu_types_dir)

  output_path = os.path.join(context.gpu_types_dir, f"{type_obj.name}__gpu.h")
  file_buffer = arapi.utils.FileBuffer(output_path)

  struct_name = f"{type_obj.name}__gpu"
  full_model_type = type_obj.get_full_type_name()

  file_buffer.append(f"""// Auto-generated GPU struct for {type_obj.name}
// DO NOT EDIT - Generated by argen.py

#pragma once

#include <gpu_types/gpu_port.h>
#include <gpu_types/gpu_push_constants.h>

#ifdef __cplusplus
#include <cstring>
#include <cstddef>
#include <cstdint>
#endif

GPU_BEGIN_NAMESPACE

gpu_struct_std140 {struct_name}
{{
    std140_uint texture_indices[KGPU_MAX_TEXTURE_SLOTS];
    std140_uint sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
""")

  for field in fields:
    gpu_field_name = field.name[2:] if field.name.startswith("m_") else field.name
    file_buffer.append(f"    {field.gpu_type} {gpu_field_name};\n")

  file_buffer.append(f"""}};

GPU_END_NAMESPACE

#ifdef __cplusplus
inline void pack__{struct_name}(
    const {full_model_type}& src,
    ::kryga::gpu::{struct_name}& dst)
{{
    const uint8_t* blob = reinterpret_cast<const uint8_t*>(&src);
    for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i) {{
        dst.texture_indices[i] = UINT32_MAX;
        dst.sampler_indices[i] = UINT32_MAX;
    }}
""")

  for field in fields:
    pack_stmt = _generate_pack_statement(field, "src", "dst", full_model_type)
    file_buffer.append(f"{pack_stmt}\n")

  file_buffer.append("""}\n#endif
""")

  file_buffer.write_if_changed()
  return True


def write_all_gpu_structs(context: arapi.types.file_context) -> None:
  """Write GPU structs for all types that have GPU fields.

  Args:
      context: File context with all types
  """
  for type_obj in context.types:
    write_gpu_struct(type_obj, context)


def type_has_gpu_data(type_obj: arapi.types.kryga_type,
                      context: arapi.types.file_context) -> bool:
  """Check if a type has GPU data fields (including inherited).

  Args:
      type_obj: The type to check
      context: File context

  Returns:
      True if the type has any GPU data fields
  """
  fields = _collect_gpu_fields_from_type(type_obj, context)
  return len(fields) > 0

# Constants for property access modes
SHOULD_HAVE_GETTER: Set[str] = {"cpp_readonly", "cpp_only", "script_readonly", "read_only", "all"}

SHOULD_HAVE_SETTER: Set[str] = {
    "cpp_writeonly", "cpp_only", "script_writeonly", "write_only", "all"
}

# Property access mode constants
ACCESS_ALL = "all"
ACCESS_READ_ONLY = "read_only"
ACCESS_WRITE_ONLY = "write_only"

# String constants
STRING_TRUE = "true"
STRING_FALSE = "false"
EMPTY_STRING = ""
# Block markers
BLOCK_START_PREFIX = "// block start "
BLOCK_END_PREFIX = "// block end "
FILE_TYPE_IDS = "type_ids.ar.h"
FILE_DEPENDENCY_TREE = "dependency_tree.ar.h"

# File extensions
FILE_EXT_AR_H = ".ar.h"
FILE_EXT_AR_CPP = ".ar.cpp"

# File names
FILE_PACKAGE_AR_H = "package.ar.h"
FILE_TYPES_RESOLVERS = "types_resolvers.ar.h"
FILE_TYPES_SCRIPT_IMPORTER = "types_script_importer.ar.h"


class WriterError(Exception):
  """Base exception for writer errors."""
  pass


class InvalidTypeKindError(WriterError):
  """Raised when an invalid type kind is encountered."""
  pass


def kind_to_string(kind: arapi.types.kryga_type_kind) -> str:
  """Convert type kind enum to string representation.
    
    Args:
        kind: The type kind enum value
        
    Returns:
        String representation of the type kind
        
    Raises:
        SystemExit: If kind is not recognized (for backward compatibility)
    """
  if kind == arapi.types.kryga_type_kind.CLASS:
    return "kryga_class"
  elif kind == arapi.types.kryga_type_kind.STRUCT:
    return "kryga_struct"
  elif kind == arapi.types.kryga_type_kind.EXTERNAL:
    return "kryga_external"
  else:
    exit(-1)


def generate_builder(should_generate: bool, name: str, extra_methods: str = "") -> str:
  """Generate builder struct code.

    Args:
        should_generate: Whether to generate the builder code
        name: Name of the builder (e.g., "types", "render_types")
        extra_methods: Additional method declarations to include

    Returns:
        Builder struct code as string, or empty string if should_generate is False
    """
  if not should_generate:
    return EMPTY_STRING

  # For types builder, we only need a forward declaration in header.
  # The full class definition (with members) goes in the .cpp file.
  # This makes the builder itself act as the PIMPL.
  if name == "types":
    return """struct package_types_builder;                                              \\
"""

  return f"""struct package_{name}_builder : public ::kryga::core::package_{name}_builder\\
{{                                                                                      \\
    public:                                                                             \\
        virtual bool build(::kryga::core::package& sp) override;                  \\
        virtual bool destroy(::kryga::core::package& sp) override;                \\
{extra_methods}}};                                                                                     \\
"""


def write_ar_class_include_file(ar_type: arapi.types.kryga_type, context: arapi.types.file_context,
                                output_dir: str) -> None:
  """Write class include file with reflection macros.
    
    Args:
        ar_type: The class type to generate include for
        context: File context with module information
        output_dir: Output directory (unused, kept for backward compatibility)
        
    Raises:
        SystemExit: If ar_type is not a CLASS (for backward compatibility)
    """
  if ar_type.kind != arapi.types.kryga_type_kind.CLASS:
    exit(-1)

  include_path = os.path.join(
      context.model_header_dir,
      os.path.basename(ar_type.name) + FILE_EXT_AR_H,
  )

  #print(f"generating : {include_path}")

  ar_class_include = arapi.utils.FileBuffer(include_path)

  # Check if this class has GPU data - if so, add forward declarations and friend
  gpu_fwd_decl = ""
  gpu_friend = ""
  if type_has_gpu_data(ar_type, context):
    struct_name = f"{ar_type.name}__gpu"
    full_type = ar_type.get_full_type_name()
    # Extract namespace for class forward declaration
    class_ns = context.full_module_name
    class_name = ar_type.name
    # Forward declarations before the macro
    gpu_fwd_decl = f"""namespace kryga::gpu {{ struct {struct_name}; }}
namespace {class_ns} {{ class {class_name}; }}
void pack__{struct_name}(const {full_type}&, ::kryga::gpu::{struct_name}&);

"""
    gpu_friend = f"    friend void ::pack__{struct_name}(const {full_type}&, ::kryga::gpu::{struct_name}&); \\\n"

  ar_class_include.append(f"""#pragma once

{gpu_fwd_decl}#define KRG_gen_meta__{ar_type.name}()   \\
    friend class package; \\
{gpu_friend}""")

  for prop in ar_type.properties:
    if prop.access in SHOULD_HAVE_GETTER:
      ar_class_include.append(f"""public: \\
{prop.type} get_{prop.name_cut}() const;\\
private: \\
""")

      if prop.access in SHOULD_HAVE_SETTER:
        ar_class_include.append(f"""public: \\
              void set_{prop.name_cut}({prop.type} v);\\
              private: \\
              """)
  ar_class_include.write_if_changed()


def write_ar_package_include_file(context: arapi.types.file_context, output_dir: str) -> None:
  """Write package include file with package macros.

    Args:
        context: File context with module information
        output_dir: Output directory (unused, kept for backward compatibility)
    """
  include_path = os.path.join(context.package_header_dir, FILE_PACKAGE_AR_H)

  #print(f"generating : {include_path}")

  ar_class_include = arapi.utils.FileBuffer(include_path)
  ar_class_include.append("#pragma once\n\n")

  # Generate per-type member method declarations for the 3 phases
  type_methods = ""
  for type_obj in context.types:
    # Phase 1: register_type_*
    type_methods += f"        void register_type_{type_obj.name}(::kryga::core::package& sp);             \\\n"
  for type_obj in context.types:
    # Phase 2: register_properties_* (only for types with properties)
    if type_obj.kind == arapi.types.kryga_type_kind.CLASS:
      type_methods += f"        void register_properties_{type_obj.name}();                               \\\n"
  for type_obj in context.types:
    # Phase 3: lua_bind_*
    if type_obj.kind != arapi.types.kryga_type_kind.EXTERNAL or type_obj.script_support:
      type_methods += f"        void lua_bind_{type_obj.name}();                                          \\\n"

  ar_class_include.append(f"""
#define KRG_gen_meta__{context.module_name}_package_model
#define KRG_gen_meta__{context.module_name}_package_render
#define KRG_gen_meta__{context.module_name}_package_builder

#if defined(KRG_build__model)
#undef  KRG_gen_meta__{context.module_name}_package_model
#define KRG_gen_meta__{context.module_name}_package_model \\
public: \\
static bool package_model_enforcer(); \\
static inline bool has_model_types = package_model_enforcer(); \\
static ::kryga::utils::id \\
package_id() \\
{{ \\
    return AID("{context.module_name}"); \\
}} \\
static auto \\
package_loader() -> std::unique_ptr<::kryga::core::package>(*)() \\
{{ \\
    return +[]() -> std::unique_ptr<::kryga::core::package> {{ \\
        auto pkg = std::make_unique<package>(); \\
        instance_impl() = pkg.get(); \\
        return pkg; \\
    }}; \\
}} \\
static package*& \\
instance_impl() \\
{{ \\
    static package* s_instance = nullptr; \\
    return s_instance; \\
}} \\
static package& \\
instance() \\
{{ \\
    KRG_check(instance_impl(), "empty instance"); \\
    return *instance_impl(); \\
}} \\
{generate_builder(True, "types", type_methods)}{generate_builder(True, "types_default_objects")} \\
private:
#endif

#if defined(KRG_build__render)
#undef  KRG_gen_meta__{context.module_name}_package_render
#define KRG_gen_meta__{context.module_name}_package_render \\
private:\\
  static bool package_render_enforcer();\\
  static inline bool has_render_types = package_render_enforcer();\\
public:\\
{generate_builder(context.render_has_types_overrides, "render_types")}{generate_builder(context.render_has_custom_resources, "render_custom_resource")} \\
private: 
#endif

#if defined(KRG_build__builder)
#undef  KRG_gen_meta__{context.module_name}_package_builder
#define KRG_gen_meta__{context.module_name}_package_builder \\

#endif
""")

  ar_class_include.append(f"""
#define KRG_gen_meta__{context.module_name}_package \\
  KRG_gen_meta__{context.module_name}_package_model \\
  KRG_gen_meta__{context.module_name}_package_render \\
  KRG_gen_meta__{context.module_name}_package_builder
""")

  ar_class_include.write_if_changed()


def write_property_access_methods(fc: arapi.types.file_context,
                                  prop: arapi.types.kryga_property) -> None:
  """Write property accessor methods (getters/setters) to context.
    
    Args:
        fc: File context to append methods to
        prop: Property to generate accessors for
    """
  if prop.access in SHOULD_HAVE_GETTER:
    fc.properies_access_methods += f"""{prop.type}
{prop.owner}::get_{prop.name_cut}() const
{{
    return m_{prop.name_cut};
}}

"""

  if prop.access in SHOULD_HAVE_SETTER:
    fc.properies_access_methods += f"""void
{prop.owner}::set_{prop.name_cut}({prop.type} v)
{{
"""
    if prop.check_not_same:
      fc.properies_access_methods += f"    if(m_{prop.name_cut} == v) {{ return; }}\n"

    fc.properies_access_methods += f"    m_{prop.name_cut} = v;\n"

    if prop.invalidates_transform:
      fc.properies_access_methods += "    mark_transform_dirty();\n    update_children_matrixes();\n"

    if prop.invalidates_render:
      fc.properies_access_methods += "    mark_render_dirty();\n"

    fc.properies_access_methods += "}\n\n"


def _write_property_reflection(file_buffer: arapi.utils.FileBuffer, fc: arapi.types.file_context,
                               prop: arapi.types.kryga_property, type_name: str) -> None:
  """Write property reflection metadata.
    
    Args:
        ar_file: File handle to write to
        fc: File context
        prop: Property to write reflection for
        type_name: Name of the owning type
    """
  file_buffer.append(f"""
    {{
        using type       = {prop.owner};

        auto property_td = ::kryga::reflection::kryga_type_resolve<decltype(type::m_{prop.name_cut})>();
        auto prop_rtype  = ::kryga::glob::glob_state().get_rm()->get_type(property_td.type_id);
        if(!prop_rtype)
        {{
            ALOG_WARN("Property doesn't have a type {prop.owner}:{prop.name_cut}");
        }}

        auto prop        = std::make_shared<::kryga::reflection::property>();
        auto p           = prop.get();

        {fc.module_name}_{type_name}_rt->m_properties.emplace_back(std::move(prop));

        // Main fields
        p->name                           = "{prop.name_cut}";
        p->offset                         = offsetof(type, m_{prop.name_cut});
        p->rtype                          = prop_rtype;
""")

  if prop.category != EMPTY_STRING:
    file_buffer.append(f"        p->category  = \"{prop.category}\";\n")

  if prop.gpu_data != EMPTY_STRING:
    file_buffer.append(f"        p->gpu_data  = \"{prop.gpu_data}\";\n")

  if prop.has_default == STRING_TRUE:
    file_buffer.append(f"        p->has_default  = {prop.has_default};\n")

  if prop.serializable == STRING_TRUE:
    file_buffer.append("        p->serializable  = true;\n")

  if prop.invalidates_render:
    file_buffer.append(f"        p->render_subobject  = std::is_base_of_v<::kryga::root::smart_object, typename std::remove_pointer_t<{prop.type}>>;\n")

  if prop.property_ser_handler != EMPTY_STRING:
    file_buffer.append(f"        p->save_handler  = {prop.property_ser_handler};\n")

  if prop.property_load_derive_handler != EMPTY_STRING:
    file_buffer.append(f"        p->load_handler  = {prop.property_load_derive_handler};\n")

  if prop.property_compare_handler != EMPTY_STRING:
    file_buffer.append(f"        p->compare_handler  = {prop.property_compare_handler};\n")

    if prop.property_copy_handler != EMPTY_STRING:
      file_buffer.append(f"        p->copy_handler  = {prop.property_copy_handler};\n")

  if prop.property_instantiate_handler != EMPTY_STRING:
    file_buffer.append(f"        p->instantiate_handler  = {prop.property_instantiate_handler};\n")

  file_buffer.append("    }\n")


def write_properties(file_buffer: arapi.utils.FileBuffer, fc: arapi.types.file_context,
                     type_obj: arapi.types.kryga_type) -> None:
  """Write properties reflection code for a type.
    
    Args:
        file_buffer: File buffer to write to
        fc: File context
        type_obj: Type to write properties for
    """
  if len(type_obj.properties) == 0:
    return

  file_buffer.append("    {\n")

  for prop in type_obj.properties:
    write_property_access_methods(fc, prop)
    _write_property_reflection(file_buffer, fc, prop, type_obj.name)

  file_buffer.append("    }\n")


def write_types_resolvers(fc: arapi.types.file_context) -> None:
  """Write type resolver template specializations.
    
    Args:
        fc: File context with types to generate resolvers for
    """
  output_file = os.path.join(fc.package_header_dir, FILE_TYPES_RESOLVERS)

  output = arapi.utils.FileBuffer(output_file)

  output.append("""#pragma once

#include <core/reflection/types.h>

#include <glue/type_ids.ar.h>

""")

  includes_list = sorted(fc.includes)

  for include in includes_list:
    output.append(include)
    output.append("\n")

  output.append("""
  namespace kryga::reflection
  {
  """)

  for type_obj in fc.types:
    output.append(f"""
  template <>
  struct type_resolver<{type_obj.get_full_type_name()}>
  {{
      enum
      {{
          value = ::kryga::{type_obj.id}
      }};
  }};
  """)

  output.append("}\n")

  output.write_if_changed()


def write_lua_class_type(file_buffer: arapi.utils.FileBuffer, fc: arapi.types.file_context,
                         type_obj: arapi.types.kryga_type) -> None:
  """Write Lua binding code for a class type.
    
    Args:
        file: File handle to write to
        fc: File context
        type_obj: Class type to generate Lua bindings for
    """
  file_buffer.append(f"""
    {{
        *{type_obj.name}_lua_type = ::kryga::glob::glob_state().get_lua()->state().new_usertype<{type_obj.get_full_type_name()}>(
        "{type_obj.name}", sol::no_constructor,
            "i",
            [](const char* id) -> {type_obj.get_full_type_name()}*
            {{
                auto item = ::kryga::glob::glob_state().get_instance_objects_cache()->get_item(AID(id));

                if(!item)
                {{
                    return nullptr;
                }}

                return item->as<{type_obj.get_full_type_name()}>();
            }},
            "c",
            [](const char* id) -> {type_obj.get_full_type_name()}*
            {{
                auto item = ::kryga::glob::glob_state().get_class_objects_cache()->get_item(AID(id));

                if(!item)
                {{
                    return nullptr;
                }}

                return item->as<{type_obj.get_full_type_name()}>();
            }}""")

  if type_obj.parent_type:
    file_buffer.append(
        f""",sol::base_classes, sol::bases<{type_obj.parent_type.get_full_type_name()}>()
               """)
    file_buffer.append(f""");
             
    {type_obj.name}__lua_script_extension<sol::usertype<{type_obj.get_full_type_name()}>, {type_obj.get_full_type_name()}>(*{type_obj.name}_lua_type);

}}""")
  else:
    file_buffer.append(f""");
    }}""")


def write_lua_struct_type(file_buffer: arapi.utils.FileBuffer, fc: arapi.types.file_context,
                          type_obj: arapi.types.kryga_type) -> None:
  """Write Lua binding code for a struct type.
    
    Args:
        file: File handle to write to
        fc: File context
        type_obj: Struct type to generate Lua bindings for
    """
  ctor_line = ",".join(ctor.name for ctor in type_obj.ctros)

  file_buffer.append(f"""
         *{type_obj.name}_lua_type = ::kryga::glob::glob_state().get_lua()->state().new_usertype<{type_obj.get_full_type_name()}>(
        "{type_obj.get_full_type_name()}", sol::constructors<{ctor_line}>());
  """)


def write_lua_usertype_extension(fc: arapi.types.file_context) -> None:
  """Write Lua user type extension functions.
    
    Args:
        fc: File context with types to generate extensions for
    """
  file_path = os.path.join(fc.package_header_dir, FILE_TYPES_SCRIPT_IMPORTER)

  file_buffer = arapi.utils.FileBuffer(file_path)
  file_buffer.append("""#pragma once

""")
  file_buffer.append("""#pragma once

""")

  for type_obj in fc.types:
    file_buffer.append(f"""template <typename T, typename K>
void
{type_obj.name}__lua_script_extension(T& lua_type)
{{
""")
    if type_obj.parent_type:
      file_buffer.append(
          f"    {type_obj.parent_type.name}__lua_script_extension<T, K>(lua_type);\n")

    for prop in type_obj.properties:
      if prop.access == ACCESS_ALL or prop.access == ACCESS_READ_ONLY:
        file_buffer.append(f"""    lua_type["get_{prop.name_cut}"] = &K::get_{prop.name_cut};
""")

      if prop.access == ACCESS_ALL or prop.access == ACCESS_WRITE_ONLY:
        file_buffer.append(f"""    lua_type["set_{prop.name_cut}"] = &K::set_{prop.name_cut};
""")

    for func in type_obj.functions:
      file_buffer.append(f"""    lua_type["{func.name}"] = &K::{func.name};
""")

    file_buffer.append(f"""}}

""")

  file_buffer.write_if_changed()


def model_generate_overrides_headers(fc: arapi.types.file_context) -> str:
  """Generate model override header includes.
    
    Args:
        fc: File context with model overrides
        
    Returns:
        String containing include statements for model overrides
    """
  overrides = ""
  for include in fc.model_overrides:
    overrides += f"#include <{include}>\n"

  return overrides


def _write_reflection_methods(fc: arapi.types.file_context, type_obj: arapi.types.kryga_type) -> str:
  """Generate reflection methods for a class type.
    
    Args:
        fc: File context
        type_obj: Class type to generate methods for
        
    Returns:
        String containing reflection method implementations
    """
  return f"""
const ::kryga::reflection::reflection_type& 
{type_obj.name}::AR_TYPE_reflection()
{{
    return *{fc.module_name}_{type_obj.name}_rt;
}}                  

std::shared_ptr<::kryga::root::smart_object>
{type_obj.name}::AR_TYPE_create_empty_gen_obj(::kryga::reflection::type_context__alloc& ctx)
{{
    return {type_obj.name}::AR_TYPE_create_empty_obj(*ctx.id);
}}

std::shared_ptr<{type_obj.name}>
{type_obj.name}::AR_TYPE_create_empty_obj(const ::kryga::utils::id& id)
{{
    auto s = std::make_shared<this_class>();
    s->META_set_reflection_type(&this_class::AR_TYPE_reflection());
    s->META_set_id(id);
    return s;
}}

std::unique_ptr<::kryga::root::base_construct_params>
{type_obj.name}::AR_TYPE_create_gen_default_cparams()
{{
    auto ptr = std::make_unique<{type_obj.name}::construct_params>();          

    return ptr;
}}

::kryga::utils::id
{type_obj.name}::AR_TYPE_id()
{{
    return AID("{type_obj.name}");
}}

bool
{type_obj.name}::META_construct(const ::kryga::root::base_construct_params& i)
{{
    /* Replace to dynamic cast */
    auto p = (this_class::construct_params*)&i;

    return construct(*p);
}}
"""


def _write_type_registration_body(file_buffer: arapi.utils.FileBuffer, fc: arapi.types.file_context,
                                   type_obj: arapi.types.kryga_type, indent: str = "    ") -> None:
  """Write the body of type registration (shared between inline and function versions).

    Args:
        file_buffer: File buffer to write to
        fc: File context
        type_obj: Type to register
        indent: Indentation prefix
  """
  file_buffer.append(f"""
{indent}const int type_id = ::kryga::reflection::type_resolver<{type_obj.get_full_type_name()}>::value;
{indent}KRG_check(type_id != -1, "Type is not defined!");
{indent}m_rt_{type_obj.name} = std::make_unique<::kryga::reflection::reflection_type>(type_id, AID("{type_obj.name}"));
{indent}{fc.module_name}_{type_obj.name}_rt = m_rt_{type_obj.name}.get();
{indent}auto& rt         = *add(*m_package, {fc.module_name}_{type_obj.name}_rt);
{indent}rt.type_id       = type_id;
{indent}rt.type_class    = ::kryga::reflection::reflection_type::reflection_type_class::{kind_to_string(type_obj.kind)};

{indent}rt.module_id     = AID("{fc.module_name}");
{indent}rt.size          = sizeof({type_obj.get_full_type_name()});
""")

  # lua_storage already contains the sol::usertype fields, no need to allocate

  if type_obj.kind == arapi.types.kryga_type_kind.CLASS:
    file_buffer.append(f"""
{indent}rt.alloc         = {type_obj.name}::AR_TYPE_create_empty_gen_obj;
{indent}rt.cparams_alloc = {type_obj.name}::AR_TYPE_create_gen_default_cparams;
""")

  if type_obj.architype:
    file_buffer.append(f"{indent}rt.arch         = core::architype::{type_obj.architype};\n")

  if type_obj.parent_type or len(type_obj.parent_name) > 0:
    parent_name = type_obj.parent_name if type_obj.parent_name else type_obj.parent_type.name
    file_buffer.append(f"""
{indent}int parent_type_id = ::kryga::reflection::type_resolver<{parent_name}>::value;
{indent}KRG_check(parent_type_id != -1, "Type is not defined!");

{indent}auto parent_rt =  ::kryga::glob::glob_state().get_rm()->get_type(parent_type_id);
{indent}KRG_check(parent_rt, "Type is not defined!");

{indent}rt.parent = parent_rt;
""")

  if type_obj.compare_handler:
    file_buffer.append(f"{indent}rt.compare                = {type_obj.compare_handler};\n")

  if type_obj.copy_handler:
    file_buffer.append(f"{indent}rt.copy                   = {type_obj.copy_handler};\n")

  if type_obj.serialize_handler:
    file_buffer.append(f"{indent}rt.save                   = {type_obj.serialize_handler};\n")

  if type_obj.load_derive_handler:
    file_buffer.append(f"{indent}rt.load                   = {type_obj.load_derive_handler};\n")

  if type_obj.to_string_handler:
    file_buffer.append(f"{indent}rt.to_string              = {type_obj.to_string_handler};\n")

  if type_obj.instantiate_handler:
    file_buffer.append(f"{indent}rt.instantiate            = {type_obj.instantiate_handler};\n")


def _write_type_registration_function(file_buffer: arapi.utils.FileBuffer, fc: arapi.types.file_context,
                                       type_obj: arapi.types.kryga_type) -> None:
  """Write type registration as a member function of package_types_builder.

    Args:
        file_buffer: File buffer to write to
        fc: File context
        type_obj: Type to register
  """
  file_buffer.append(f"""
void
package::package_types_builder::register_type_{type_obj.name}()
{{""")
  _write_type_registration_body(file_buffer, fc, type_obj, "    ")
  file_buffer.append("}\n")


def _write_properties_member_function(file_buffer: arapi.utils.FileBuffer, fc: arapi.types.file_context,
                                       type_obj: arapi.types.kryga_type) -> None:
  """Write properties registration as a member function of package_types_builder.

    Args:
        file_buffer: File buffer to write to
        fc: File context
        type_obj: Type to register properties for
  """

  file_buffer.append(f"""
void
package::package_types_builder::register_properties_{type_obj.name}()
{{
""")

  for prop in type_obj.properties:
    write_property_access_methods(fc, prop)
    _write_property_reflection(file_buffer, fc, prop, type_obj.name)

  file_buffer.append("}\n")


def _write_lua_binding_function(file_buffer: arapi.utils.FileBuffer, fc: arapi.types.file_context,
                                 type_obj: arapi.types.kryga_type) -> None:
  """Write Lua binding as a member function of package_types_builder.

    Args:
        file_buffer: File buffer to write to
        fc: File context
        type_obj: Type to generate Lua bindings for
  """
  file_buffer.append(f"""
void
package::package_types_builder::lua_bind_{type_obj.name}()
{{
""")

  if type_obj.kind == arapi.types.kryga_type_kind.CLASS:
    # Write lua class type binding inline
    file_buffer.append(f"""    m_lua_{type_obj.name} = ::kryga::glob::glob_state().get_lua()->state().new_usertype<{type_obj.get_full_type_name()}>(
    "{type_obj.name}", sol::no_constructor,
        "i",
        [](const char* id) -> {type_obj.get_full_type_name()}*
        {{
            auto item = ::kryga::glob::glob_state().get_instance_objects_cache()->get_item(AID(id));

            if(!item)
            {{
                return nullptr;
            }}

            return item->as<{type_obj.get_full_type_name()}>();
        }},
        "c",
        [](const char* id) -> {type_obj.get_full_type_name()}*
        {{
            auto item = ::kryga::glob::glob_state().get_class_objects_cache()->get_item(AID(id));

            if(!item)
            {{
                return nullptr;
            }}

            return item->as<{type_obj.get_full_type_name()}>();
        }}""")

    if type_obj.parent_type:
      file_buffer.append(
          f""",sol::base_classes, sol::bases<{type_obj.parent_type.get_full_type_name()}>()
           """)
      file_buffer.append(f""");

    {type_obj.name}__lua_script_extension<sol::usertype<{type_obj.get_full_type_name()}>, {type_obj.get_full_type_name()}>(m_lua_{type_obj.name});
""")
    else:
      file_buffer.append(""");
""")

  elif type_obj.kind == arapi.types.kryga_type_kind.STRUCT:
    ctor_line = ",".join(ctor.name for ctor in type_obj.ctros)
    file_buffer.append(f"""    m_lua_{type_obj.name} = ::kryga::glob::glob_state().get_lua()->state().new_usertype<{type_obj.get_full_type_name()}>(
    "{type_obj.get_full_type_name()}", sol::constructors<{ctor_line}>());
""")

  file_buffer.append("}\n")


def write_types_builder_header(header_file: str, fc: arapi.types.file_context) -> None:
  """Write package_types_builder header file.

    Generates a header with the full class definition for package_types_builder.
    This can be included by tests or other code that needs to instantiate the builder.

    Args:
        header_file: Path to output header file
        fc: File context with all type information
  """
  file_buffer = arapi.utils.FileBuffer(header_file)

  # Generate member declarations
  rt_members = ""
  for type_obj in fc.types:
    rt_members += f"    std::unique_ptr<::kryga::reflection::reflection_type> m_rt_{type_obj.name};\n"

  lua_members = ""
  for type_obj in fc.types:
    if type_obj.kind != arapi.types.kryga_type_kind.EXTERNAL or type_obj.script_support:
      lua_members += f"    sol::usertype<{type_obj.get_full_type_name()}> m_lua_{type_obj.name};\n"

  # Generate per-type method declarations
  type_method_decls = ""
  for type_obj in fc.types:
    type_method_decls += f"    void register_type_{type_obj.name}();\n"
  for type_obj in fc.types:
    if type_obj.kind == arapi.types.kryga_type_kind.CLASS:
      type_method_decls += f"    void register_properties_{type_obj.name}();\n"
  for type_obj in fc.types:
    if type_obj.kind != arapi.types.kryga_type_kind.EXTERNAL or type_obj.script_support:
      type_method_decls += f"    void lua_bind_{type_obj.name}();\n"

  # Collect type includes (needed for sol::usertype<T> to have complete types)
  type_includes = ""
  includes_list = sorted(fc.includes)
  for include in includes_list:
    type_includes += include + "\n"

  # Collect dependency resolver includes
  dependency_includes = ""
  for dep in fc.dependencies:
    dependency_includes += f'#include "packages/{dep}/types_resolvers.ar.h"\n'

  file_buffer.append(f"""// Auto-generated package_types_builder definition
// Include this header when you need to instantiate the builder (e.g., in tests)

#pragma once

// clang-format off

#include "packages/{fc.module_name}/package.{fc.module_name}.h"
#include "packages/{fc.module_name}/types_resolvers.ar.h"
{dependency_includes}
{type_includes}
#include <core/package.h>
#include <core/reflection/reflection_type.h>
#include <sol2_unofficial/sol.h>

#include <memory>

namespace kryga::{fc.module_name} {{

struct package::package_types_builder : public ::kryga::core::package_types_builder
{{
public:
    bool build(::kryga::core::package& sp) override;
    bool destroy(::kryga::core::package& sp) override;

private:
{type_method_decls}
    // Package context (set in build())
    ::kryga::core::package* m_package = nullptr;

    // Reflection type storage
{rt_members}
    // Lua usertype storage
{lua_members}}};

}} // namespace kryga::{fc.module_name}
""")

  file_buffer.write_if_changed()


def write_object_model_reflection(package_ar_file: str, fc: arapi.types.file_context) -> None:
  """Write object model reflection implementation file.

    This is the main function that generates the complete model reflection
    source file including type registrations, property reflections, and Lua bindings.

    Args:
        package_ar_file: Path to output file
        fc: File context with all type information
    """
  file_buffer = arapi.utils.FileBuffer(package_ar_file)
  model_conditional_header = model_generate_overrides_headers(fc)

  external_dependencies = ""

  for dep in fc.dependencies:
    external_dependencies += f'#include "packages/{dep}/types_resolvers.ar.h"\n'

  file_buffer.append(f"""// Smart Object Autogenerated Reflection Layout

// clang-format off

#include "glue/type_ids.ar.h"

#include "packages/{fc.module_name}/package.{fc.module_name}.h"
#include "packages/{fc.module_name}/types_resolvers.ar.h"
#include "packages/{fc.module_name}/types_script_importer.ar.h"
{model_conditional_header}{external_dependencies}
#include <core/caches/caches_map.h>
#include <core/reflection/reflection_type.h>
#include <core/reflection/reflection_type_utils.h>
#include <core/reflection/lua_api.h>
#include <core/object_constructor.h>
#include <core/package_manager.h>
#include <core/package.h>
#include <core/global_state.h>

#include <sol2_unofficial/sol.h>

namespace kryga::{fc.module_name} {{

""")

  # Write static raw pointer declarations for reflection types
  for type_obj in fc.types:
    file_buffer.append(f"""
static ::kryga::reflection::reflection_type* {fc.module_name}_{type_obj.name}_rt = nullptr;""")

  # Generate member declarations for the class
  # Use m_rt_ prefix for reflection types to avoid C++ keyword conflicts (bool, float, etc.)
  rt_members = ""
  for type_obj in fc.types:
    rt_members += f"    std::unique_ptr<::kryga::reflection::reflection_type> m_rt_{type_obj.name};\n"

  # Use m_lua_ prefix for lua types
  lua_members = ""
  for type_obj in fc.types:
    if type_obj.kind != arapi.types.kryga_type_kind.EXTERNAL or type_obj.script_support:
      lua_members += f"    sol::usertype<{type_obj.get_full_type_name()}> m_lua_{type_obj.name};\n"

  # Generate per-type method declarations (no package param - use m_package member)
  type_method_decls = ""
  for type_obj in fc.types:
    type_method_decls += f"    void register_type_{type_obj.name}();\n"
  for type_obj in fc.types:
    if type_obj.kind == arapi.types.kryga_type_kind.CLASS:
      type_method_decls += f"    void register_properties_{type_obj.name}();\n"
  for type_obj in fc.types:
    if type_obj.kind != arapi.types.kryga_type_kind.EXTERNAL or type_obj.script_support:
      type_method_decls += f"    void lua_bind_{type_obj.name}();\n"

  # Write the full class definition - the builder IS the pimpl
  file_buffer.append(f"""

struct package::package_types_builder : public ::kryga::core::package_types_builder
{{
public:
    bool build(::kryga::core::package& sp) override;
    bool destroy(::kryga::core::package& sp) override;

private:
{type_method_decls}
    // Package context (set in build())
    ::kryga::core::package* m_package = nullptr;

    // Reflection type storage
{rt_members}
    // Lua usertype storage
{lua_members}}};
""")

  # Generate reflection methods for classes
  reflection_methods = ""
  for type_obj in fc.types:
    if type_obj.kind == arapi.types.kryga_type_kind.CLASS:
      reflection_methods += _write_reflection_methods(fc, type_obj)

  file_buffer.append("\n\n")
  file_buffer.append(reflection_methods)

  # Write per-type static functions for 3-phase build

  # Phase 1: Write register_type_* functions
  for type_obj in fc.types:
    _write_type_registration_function(file_buffer, fc, type_obj)

  # Phase 2: Write register_properties_* member functions
  for type_obj in fc.types:
    if type_obj.kind == arapi.types.kryga_type_kind.CLASS:
      _write_properties_member_function(file_buffer, fc, type_obj)

  # Phase 3: Write lua_bind_* functions
  for type_obj in fc.types:
    if type_obj.kind != arapi.types.kryga_type_kind.EXTERNAL or type_obj.script_support:
      _write_lua_binding_function(file_buffer, fc, type_obj)

  # Write static schedule calls
  file_buffer.append(f"""
KRG_gen__static_schedule(::kryga::gs::state::state_stage::create,
    [](kryga::gs::state& s)
    {{                     
        s.get_pm()->register_static_package_loader<package>();
        auto& pkg = s.get_pm()->load_static_package<package>();

    }});

KRG_gen__static_schedule(::kryga::gs::state::state_stage::connect,
    [](kryga::gs::state& s)
    {{
       {fc.module_name}::package::instance().register_package_extension<package::package_types_builder>();
       {fc.module_name}::package::instance().register_package_extension<package::package_types_default_objects_builder>();
    }});

bool package::package_model_enforcer()
{{
  volatile bool has_render_types = false;
  return has_render_types;
}}

bool
package::package_types_builder::build(::kryga::core::package& sp)
{{
    m_package = &sp;

    // Phase 1: Register types
""")

  for type_obj in fc.types:
    file_buffer.append(f"    register_type_{type_obj.name}();\n")

  file_buffer.append("""
    // Phase 2: Register properties
""")

  for type_obj in fc.types:
    if type_obj.kind == arapi.types.kryga_type_kind.CLASS:
      file_buffer.append(f"    register_properties_{type_obj.name}();\n")

  file_buffer.append("""
    // Phase 3: Lua bindings
""")

  for type_obj in fc.types:
    if type_obj.kind != arapi.types.kryga_type_kind.EXTERNAL or type_obj.script_support:
      file_buffer.append(f"    lua_bind_{type_obj.name}();\n")

  file_buffer.append("""
    return true;
}
""")
  file_buffer.append(f"""
bool
package::package_types_builder::destroy(::kryga::core::package& sp)
{{
""")
  # Clear raw pointers and reset members
  for type_obj in reversed(fc.types):
    file_buffer.append(f"    {fc.module_name}_{type_obj.name}_rt = nullptr;\n")
    file_buffer.append(f"    m_rt_{type_obj.name}.reset();\n")

  file_buffer.append(f"""    return true;
}}
bool
package::package_types_default_objects_builder::build(::kryga::core::package& sp)
{{
    auto pkg = &::kryga::{fc.module_name}::package::instance();
""")

  file_buffer.append("\n    return true;\n}\n\n")

  file_buffer.append(f"""
bool
package::package_types_default_objects_builder::destroy(::kryga::core::package& sp)
{{
    auto pkg = &::kryga::{fc.module_name}::package::instance();
""")

  for type_obj in fc.types:
    if type_obj.kind == arapi.types.kryga_type_kind.CLASS:
      file_buffer.append(f"    pkg->destroy_default_class_obj<{type_obj.get_full_type_name()}>();\n")

  file_buffer.append("\n    return true;\n}\n\n")

  file_buffer.append(fc.properies_access_methods)

  file_buffer.append("\n}\n")

  file_buffer.write_if_changed()

  write_types_resolvers(fc)
  write_lua_usertype_extension(fc)


def write_render_types_reflection(package_ar_file: str, fc: arapi.types.file_context) -> None:
  """Write render types reflection implementation file.
    
    Args:
        package_ar_file: Path to output file
        fc: File context with render type information
    """

  file_buffer = arapi.utils.FileBuffer(package_ar_file)
  render_overrides = ""
  for include in fc.render_overrides:
    render_overrides += f"#include <{include}>\n"

  # Generate GPU type includes
  gpu_includes = ""
  for type_obj in fc.types:
    if type_obj.kind == arapi.types.kryga_type_kind.CLASS and type_has_gpu_data(type_obj, fc):
      gpu_includes += f'#include <gpu_types/{type_obj.name}__gpu.h>\n'

  file_buffer.append(f"""// Smart Object Autogenerated Reflection Layout

// clang-format off


#include "packages/{fc.module_name}/package.{fc.module_name}.h"
#include "packages/{fc.module_name}/types_resolvers.ar.h"
#include "packages/{fc.module_name}/types_script_importer.ar.h"

{render_overrides}
#include <core/caches/caches_map.h>
#include <core/reflection/reflection_type.h>
#include <core/reflection/reflection_type_utils.h>
#include <core/reflection/lua_api.h>
#include <core/object_constructor.h>
#include <core/package_manager.h>
#include <core/package.h>
#include <global_state/global_state.h>
#include <glue/type_ids.ar.h>

{gpu_includes}
namespace kryga::{fc.module_name} {{

""")

  # Generate static GPU pack wrapper functions
  for type_obj in fc.types:
    if type_obj.kind == arapi.types.kryga_type_kind.CLASS and type_has_gpu_data(type_obj, fc):
      struct_name = f"{type_obj.name}__gpu"
      full_type = type_obj.get_full_type_name()
      file_buffer.append(f"""
static void gpu_pack__{type_obj.name}(const void* src, void* dst)
{{
    pack__{struct_name}(
        *static_cast<const {full_type}*>(src),
        *static_cast<::kryga::gpu::{struct_name}*>(dst));
}}
""")

  file_buffer.append(f"""
bool package::package_render_enforcer()
{{
  volatile bool has_render_types = false;
  return has_render_types;
}}
""")

  if fc.render_has_custom_resources:
    file_buffer.append("""
KRG_gen__static_schedule(::kryga::gs::state::state_stage::connect,
    [](::kryga::gs::state& s)
    {{
      package::instance().register_package_extension<package::package_render_custom_resource_builder>(); 
    }});
""")

  if fc.render_has_types_overrides:
    file_buffer.append("""
KRG_gen__static_schedule(::kryga::gs::state::state_stage::connect,
    [](::kryga::gs::state& s)
    {{
      package::instance().register_package_extension<package::package_render_types_builder>(); 
    }});
""")

  if fc.render_has_types_overrides:
    file_buffer.append(f"""
bool
package::package_render_types_builder::build(::kryga::core::package& sp)
{{
  auto pkg = &::kryga::{fc.module_name}::package::instance();
""")

    for type_obj in fc.types:
      has_render_handlers = type_obj.render_constructor or type_obj.render_destructor
      has_gpu_data = type_has_gpu_data(type_obj, fc)

      if (type_obj.kind == arapi.types.kryga_type_kind.CLASS
          and (has_render_handlers or has_gpu_data)):

        file_buffer.append(f"""
{{
  auto type_rt =  ::kryga::glob::glob_state().get_rm()->get_type(::kryga::{type_obj.id});
  KRG_check(type_rt, "Type is not defined!");
""")
        if type_obj.render_constructor:
          file_buffer.append(f"  type_rt->render_constructor = {type_obj.render_constructor};\n")
        if type_obj.render_destructor:
          file_buffer.append(f"  type_rt->render_destructor  = {type_obj.render_destructor};\n")
        if has_gpu_data:
          struct_name = f"{type_obj.name}__gpu"
          file_buffer.append(f"  type_rt->gpu_pack           = gpu_pack__{type_obj.name};\n")
          file_buffer.append(f"  type_rt->gpu_data_size      = sizeof(::kryga::gpu::{struct_name});\n")
        file_buffer.append("}")

    file_buffer.append(f"""
  return true;
}}
bool
package::package_render_types_builder::destroy(::kryga::core::package&)
{{
  return true;
}}
                  
}}""")

  file_buffer.write_if_changed()


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

namespace kryga {
  enum {
// block start zzero
    kryga__total_supported_types_number,
    kryga__invalid_type_id = kryga__total_supported_types_number
// block end zzero
  };
}
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
  file_buffer = arapi.utils.FileBuffer(global_file)
  file_buffer.append(lines)
  file_buffer.write_if_changed()


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

namespace kryga
{

std::vector<utils::id>
get_dependency(const utils::id& package_id)
{
    // block start root
    if (package_id == AID("root"))
    {
        return {};
    }
    // block end root
    return {};
}

}  // namespace kryga
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
