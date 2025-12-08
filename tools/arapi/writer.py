"""Code generation module for AGEA reflection system.

This module generates C++ header and source files for reflection metadata,
including class includes, package includes, type resolvers, Lua bindings,
and reflection implementations.
"""
import os
from typing import TextIO, Set, List, Optional, Tuple, Dict
from collections import OrderedDict, deque
import arapi.types

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


def kind_to_string(kind: arapi.types.agea_type_kind) -> str:
  """Convert type kind enum to string representation.
    
    Args:
        kind: The type kind enum value
        
    Returns:
        String representation of the type kind
        
    Raises:
        SystemExit: If kind is not recognized (for backward compatibility)
    """
  if kind == arapi.types.agea_type_kind.CLASS:
    return "agea_class"
  elif kind == arapi.types.agea_type_kind.STRUCT:
    return "agea_struct"
  elif kind == arapi.types.agea_type_kind.EXTERNAL:
    return "agea_external"
  else:
    exit(-1)


def generate_builder(should_generate: bool, name: str) -> str:
  """Generate builder struct code.
    
    Args:
        should_generate: Whether to generate the builder code
        name: Name of the builder (e.g., "types", "render_types")
        
    Returns:
        Builder struct code as string, or empty string if should_generate is False
    """
  if not should_generate:
    return EMPTY_STRING

  return f"""struct package_{name}_builder : public ::agea::core::package_{name}_builder\\
{{                                                                                      \\
    public:                                                                             \\
        virtual bool build(::agea::core::static_package& sp) override;                  \\
        virtual bool destroy(::agea::core::static_package& sp) override;                \\
}};                                                                                     \\
"""


def write_ar_class_include_file(ar_type: arapi.types.agea_type, context: arapi.types.file_context,
                                output_dir: str) -> None:
  """Write class include file with reflection macros.
    
    Args:
        ar_type: The class type to generate include for
        context: File context with module information
        output_dir: Output directory (unused, kept for backward compatibility)
        
    Raises:
        SystemExit: If ar_type is not a CLASS (for backward compatibility)
    """
  if ar_type.kind != arapi.types.agea_type_kind.CLASS:
    exit(-1)

  include_path = os.path.join(
      context.model_header_dir,
      os.path.basename(ar_type.name) + FILE_EXT_AR_H,
  )

  print(f"generating : {include_path}")

  ar_class_include = arapi.utils.FileBuffer(include_path)
  ar_class_include.append(f"""#pragma once

#define AGEA_gen_meta__{ar_type.name}()   \\
    friend class package; \\
""")

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

  print(f"generating : {include_path}")

  ar_class_include = arapi.utils.FileBuffer(include_path)
  ar_class_include.append("#pragma once\n\n")

  ar_class_include.append(f"""
#define AGEA_gen_meta__{context.module_name}_package_model                        
#define AGEA_gen_meta__{context.module_name}_package_render
#define AGEA_gen_meta__{context.module_name}_package_builder

#if defined(AGEA_build__model)
#undef  AGEA_gen_meta__{context.module_name}_package_model
#define AGEA_gen_meta__{context.module_name}_package_model \\
public: \\
static bool package_model_enforcer(); \\
static inline bool has_model_types = package_model_enforcer(); \\
static void \\
reset_instance() \\
{{ \\
    instance_impl().reset(); \\
}} \\
static void \\
init_instance() \\
{{ \\
    AGEA_check(!instance_impl(), "using on existed"); \\
    instance_impl() = std::make_unique<package>(); \\
}} \\
static std::unique_ptr<package>& \\
instance_impl() \\
{{ \\
    static auto instance = std::make_unique<package>(); \\
    return instance; \\
}} \\
static package& \\
instance() \\
{{ \\
    AGEA_check(instance_impl(), "empty instance"); \\
    return *instance_impl(); \\
}} \\
{generate_builder(True, "types")}{generate_builder(True, "types_default_objects")} \\
private:
#endif

#if defined(AGEA_build__render)
#undef  AGEA_gen_meta__{context.module_name}_package_render
#define AGEA_gen_meta__{context.module_name}_package_render \\
private:\\
  static bool package_render_enforcer();\\
  static inline bool has_render_types = package_render_enforcer();\\
public:\\
{generate_builder(context.render_has_types_overrides, "render_types")}{generate_builder(context.render_has_custom_resources, "render_custom_resource")} \\
private: 
#endif

#if defined(AGEA_build__builder)
#undef  AGEA_gen_meta__{context.module_name}_package_builder
#define AGEA_gen_meta__{context.module_name}_package_builder \\

#endif
""")

  ar_class_include.append(f"""
#define AGEA_gen_meta__{context.module_name}_package \\
  AGEA_gen_meta__{context.module_name}_package_model \\
  AGEA_gen_meta__{context.module_name}_package_render \\
  AGEA_gen_meta__{context.module_name}_package_builder
""")

  ar_class_include.write_if_changed()


def write_property_access_methods(fc: arapi.types.file_context,
                                  prop: arapi.types.agea_property) -> None:
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
                               prop: arapi.types.agea_property, type_name: str) -> None:
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

        auto property_td = ::agea::reflection::agea_type_resolve<decltype(type::m_{prop.name_cut})>();
        auto prop_rtype  = ::agea::glob::glob_state().get_rm()->get_type(property_td.type_id);

        auto prop        = std::make_shared<::agea::reflection::property>();
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
    file_buffer.append(f"        p->render_subobject  = std::is_base_of_v<::agea::root::smart_object, typename std::remove_pointer_t<{prop.type}>>;\n")

  if prop.property_ser_handler != EMPTY_STRING:
    file_buffer.append(f"        p->serialization_handler  = {prop.property_ser_handler};\n")

  if prop.property_des_handler != EMPTY_STRING:
    file_buffer.append(f"        p->deserialization_handler  = {prop.property_des_handler};\n")

  if prop.property_load_derive_handler != EMPTY_STRING:
    file_buffer.append(f"        p->load_derive  = {prop.property_load_derive_handler};\n")

  if prop.property_compare_handler != EMPTY_STRING:
    file_buffer.append(f"        p->compare_handler  = {prop.property_compare_handler};\n")

    if prop.property_copy_handler != EMPTY_STRING:
      file_buffer.append(f"        p->copy_handler  = {prop.property_copy_handler};\n")

  if prop.property_instantiate_handler != EMPTY_STRING:
    file_buffer.append(f"        p->instantiate_handler  = {prop.property_instantiate_handler};\n")

  file_buffer.append("    }\n")


def write_properties(file_buffer: arapi.utils.FileBuffer, fc: arapi.types.file_context,
                     type_obj: arapi.types.agea_type) -> None:
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
  namespace agea::reflection
  {
  """)

  for type_obj in fc.types:
    output.append(f"""
  template <>
  struct type_resolver<{type_obj.get_full_type_name()}>
  {{
      enum
      {{
          value = ::agea::{type_obj.id}
      }};
  }};
  """)

  output.append("}\n")

  output.write_if_changed()


def write_lua_class_type(file_buffer: arapi.utils.FileBuffer, fc: arapi.types.file_context,
                         type_obj: arapi.types.agea_type) -> None:
  """Write Lua binding code for a class type.
    
    Args:
        file: File handle to write to
        fc: File context
        type_obj: Class type to generate Lua bindings for
    """
  file_buffer.append(f"""
    {{
        *{type_obj.name}_lua_type = ::agea::glob::glob_state().get_lua()->state().new_usertype<{type_obj.get_full_type_name()}>(
        "{type_obj.name}", sol::no_constructor,
            "i",
            [](const char* id) -> {type_obj.get_full_type_name()}*
            {{
                auto item = ::agea::glob::glob_state().get_instance_objects_cache()->get_item(AID(id));

                if(!item)
                {{
                    return nullptr;
                }}

                return item->as<{type_obj.get_full_type_name()}>();
            }},
            "c",
            [](const char* id) -> {type_obj.get_full_type_name()}*
            {{
                auto item = ::agea::glob::glob_state().get_class_objects_cache()->get_item(AID(id));

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
             
    {type_obj.name}__lua_script_extention<sol::usertype<{type_obj.get_full_type_name()}>, {type_obj.get_full_type_name()}>(*{type_obj.name}_lua_type);

}}""")
  else:
    file_buffer.append(f""");
    }}""")


def write_lua_struct_type(file_buffer: arapi.utils.FileBuffer, fc: arapi.types.file_context,
                          type_obj: arapi.types.agea_type) -> None:
  """Write Lua binding code for a struct type.
    
    Args:
        file: File handle to write to
        fc: File context
        type_obj: Struct type to generate Lua bindings for
    """
  ctor_line = ",".join(ctor.name for ctor in type_obj.ctros)

  file_buffer.append(f"""
         *{type_obj.name}_lua_type = ::agea::glob::glob_state().get_lua()->state().new_usertype<{type_obj.get_full_type_name()}>(
        "{type_obj.get_full_type_name()}", sol::constructors<{ctor_line}>());
  """)


def write_lua_usertype_extention(fc: arapi.types.file_context) -> None:
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
{type_obj.name}__lua_script_extention(T& lua_type)
{{
""")
    if type_obj.parent_type:
      file_buffer.append(
          f"    {type_obj.parent_type.name}__lua_script_extention<T, K>(lua_type);\n")

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


def _write_reflection_methods(fc: arapi.types.file_context, type_obj: arapi.types.agea_type) -> str:
  """Generate reflection methods for a class type.
    
    Args:
        fc: File context
        type_obj: Class type to generate methods for
        
    Returns:
        String containing reflection method implementations
    """
  return f"""
const ::agea::reflection::reflection_type& 
{type_obj.name}::AR_TYPE_reflection()
{{
    return *{fc.module_name}_{type_obj.name}_rt;
}}                  

std::shared_ptr<::agea::root::smart_object> 
{type_obj.name}::AR_TYPE_create_empty_gen_obj(const ::agea::utils::id& id)
{{    
    return {type_obj.name}::AR_TYPE_create_empty_obj(id);
}}

std::shared_ptr<{type_obj.name}>
{type_obj.name}::AR_TYPE_create_empty_obj(const ::agea::utils::id& id)
{{
    auto s = std::make_shared<this_class>();
    s->META_set_reflection_type(&this_class::AR_TYPE_reflection());
    s->META_set_id(id);
    return s;
}}

std::unique_ptr<::agea::root::base_construct_params>
{type_obj.name}::AR_TYPE_create_gen_default_cparams()
{{
    auto ptr = std::make_unique<{type_obj.name}::construct_params>();          

    return ptr;
}}

::agea::utils::id
{type_obj.name}::AR_TYPE_id()
{{
    return AID("{type_obj.name}");
}}

bool
{type_obj.name}::META_construct(const ::agea::root::base_construct_params& i)
{{
    /* Replace to dynamic cast */
    auto p = (this_class::construct_params*)&i;

    return construct(*p);
}}
"""


def _write_type_registration(file_buffer: arapi.utils.FileBuffer, fc: arapi.types.file_context,
                             type_obj: arapi.types.agea_type) -> None:
  """Write type registration code for a type.
    
    Args:
        new_content: String to write to
        fc: File context
        type_obj: Type to register
    """
  file_buffer.append(f"""
{{      
    const int type_id = ::agea::reflection::type_resolver<{type_obj.get_full_type_name()}>::value;
    AGEA_check(type_id != -1, "Type is not defined!");
    {fc.module_name}_{type_obj.name}_rt = std::make_unique<::agea::reflection::reflection_type>(type_id, AID("{type_obj.name}"));
    auto& rt         = *add(sp, {fc.module_name}_{type_obj.name}_rt.get());
    rt.type_id       = type_id;
    rt.type_class    = ::agea::reflection::reflection_type::reflection_type_class::{kind_to_string(type_obj.kind)};

    rt.module_id     = AID("{fc.module_name}");
    rt.size          = sizeof({type_obj.get_full_type_name()});
""")

  if type_obj.kind != arapi.types.agea_type_kind.EXTERNAL or type_obj.script_support:
    file_buffer.append(f"""
        {type_obj.name}_lua_type = std::make_unique<sol::usertype<{type_obj.get_full_type_name()}>>();\n
""")

  if type_obj.kind == arapi.types.agea_type_kind.CLASS:
    file_buffer.append(f"""
    rt.alloc         = {type_obj.name}::AR_TYPE_create_empty_gen_obj;
    rt.cparams_alloc = {type_obj.name}::AR_TYPE_create_gen_default_cparams;
""")

  if type_obj.architype:
    file_buffer.append(f"    rt.arch         = core::architype::{type_obj.architype};\n")

  if type_obj.parent_type or len(type_obj.parent_name) > 0:
    parent_name = type_obj.parent_name if type_obj.parent_name else type_obj.parent_type.name
    file_buffer.append(f"""
    int parent_type_id = ::agea::reflection::type_resolver<{parent_name}>::value;
    AGEA_check(parent_type_id != -1, "Type is not defined!");

    auto parent_rt =  ::agea::glob::glob_state().get_rm()->get_type(parent_type_id);
    AGEA_check(parent_rt, "Type is not defined!");

    rt.parent = parent_rt;
""")

  if type_obj.compare_handler:
    file_buffer.append(f"    rt.compare                = {type_obj.compare_handler};\n")

  if type_obj.copy_handler:
    file_buffer.append(f"    rt.copy                   = {type_obj.copy_handler};\n")

  if type_obj.serialize_handler:
    file_buffer.append(f"    rt.serialize              = {type_obj.serialize_handler};\n")

  if type_obj.deserialize_handler:
    file_buffer.append(f"    rt.deserialize            = {type_obj.deserialize_handler};\n")

  if type_obj.to_string_handle:
    file_buffer.append(f"    rt.to_string              = {type_obj.to_string_handle};\n")

  if type_obj.instantiate_handler:
    file_buffer.append(f"    rt.instantiate            = {type_obj.instantiate_handler};\n")

  file_buffer.append("}\n")


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

namespace agea::{fc.module_name} {{

""")

  # Write static variable declarations
  for type_obj in fc.types:
    file_buffer.append(f"""
static std::unique_ptr<::agea::reflection::reflection_type> {fc.module_name}_{type_obj.name}_rt;""")

    if type_obj.kind != arapi.types.agea_type_kind.EXTERNAL or type_obj.script_support:
      file_buffer.append(f"""
static std::unique_ptr<sol::usertype<{type_obj.get_full_type_name()}>> {type_obj.name}_lua_type;""")

    # Generate reflection methods for classes
    reflection_methods = ""
    for type_obj in fc.types:
      if type_obj.kind == arapi.types.agea_type_kind.CLASS:
        reflection_methods += _write_reflection_methods(fc, type_obj)

  file_buffer.append("\n\n")
  file_buffer.append(reflection_methods)

  # Write static schedule calls
  file_buffer.append(f"""
AGEA_gen__static_schedule(::agea::gs::state::state_stage::create,
    [](agea::gs::state& s)
    {{
      package::instance().register_package_extention<package::package_types_builder>();
      package::instance().register_package_extention<package::package_types_default_objects_builder>();
    }});
                  
AGEA_gen__static_schedule(::agea::gs::state::state_stage::connect,
    [](agea::gs::state& s)
    {{
      s.get_pm()->register_static_package({fc.module_name}::package::instance());
    }});

bool package::package_model_enforcer()
{{
  volatile bool has_render_types = false;
  return has_render_types;
}}

bool
package::package_types_builder::build(static_package& sp)
{{
    auto pkg = &::agea::{fc.module_name}::package::instance();
""")

  # Write type registrations
  for type_obj in fc.types:
    _write_type_registration(file_buffer, fc, type_obj)

  # Write properties
  for type_obj in fc.types:
    if type_obj.kind == arapi.types.agea_type_kind.CLASS:
      write_properties(file_buffer, fc, type_obj)

    # Write Lua bindings
  for type_obj in fc.types:
    if type_obj.kind == arapi.types.agea_type_kind.CLASS:
      write_lua_class_type(file_buffer, fc, type_obj)
    elif type_obj.kind == arapi.types.agea_type_kind.STRUCT:
      write_lua_struct_type(file_buffer, fc, type_obj)

  file_buffer.append("\n")

  # Write destroy method
  file_buffer.append("\n    return true;\n}\n\n")
  file_buffer.append(f"""
bool
package::package_types_builder::destroy(static_package& sp)
{{
""")
  for type_obj in reversed(fc.types):
    file_buffer.append(f"  {fc.module_name}_{type_obj.name}_rt.reset();\n")

    if type_obj.kind != arapi.types.agea_type_kind.EXTERNAL or type_obj.script_support:
      file_buffer.append(f"  {type_obj.name}_lua_type.reset();\n")

  file_buffer.append(f"""   return true;
}}
bool
package::package_types_default_objects_builder::build(static_package& sp)
{{
    auto pkg = &::agea::{fc.module_name}::package::instance();
""")

  for type_obj in fc.types:
    if type_obj.kind == arapi.types.agea_type_kind.CLASS:
      file_buffer.append(f"    pkg->create_default_class_obj<{type_obj.get_full_type_name()}>();\n")

  file_buffer.append("\n    return true;\n}\n\n")

  file_buffer.append(f"""
bool
package::package_types_default_objects_builder::destroy(static_package& sp)
{{
    auto pkg = &::agea::{fc.module_name}::package::instance();
""")

  for type_obj in fc.types:
    if type_obj.kind == arapi.types.agea_type_kind.CLASS:
      file_buffer.append(f"    pkg->destroy_default_class_obj<{type_obj.get_full_type_name()}>();\n")

  file_buffer.append("\n    return true;\n}\n\n")

  file_buffer.append(fc.properies_access_methods)

  file_buffer.append("\n}\n")

  file_buffer.write_if_changed()

  write_types_resolvers(fc)
  write_lua_usertype_extention(fc)


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

namespace agea::{fc.module_name} {{

bool package::package_render_enforcer()
{{
  volatile bool has_render_types = false;
  return has_render_types;
}}
""")

  if fc.render_has_custom_resources:
    file_buffer.append("""
AGEA_gen__static_schedule(::agea::gs::state::state_stage::connect,
    [](::agea::gs::state& s)
    {{
      package::instance().register_package_extention<package::package_render_custom_resource_builder>(); 
    }});
""")

  if fc.render_has_types_overrides:
    file_buffer.append("""
AGEA_gen__static_schedule(::agea::gs::state::state_stage::connect,
    [](::agea::gs::state& s)
    {{
      package::instance().register_package_extention<package::package_render_types_builder>(); 
    }});
""")

  if fc.render_has_types_overrides:
    file_buffer.append(f"""
bool
package::package_render_types_builder::build(::agea::core::static_package& sp)
{{
  auto pkg = &::agea::{fc.module_name}::package::instance();
""")

    for type_obj in fc.types:
      if (type_obj.kind == arapi.types.agea_type_kind.CLASS
          and (type_obj.render_constructor or type_obj.render_destructor)):

        file_buffer.append(f"""
{{                  
  auto type_rt =  ::agea::glob::glob_state().get_rm()->get_type(::agea::{type_obj.id});
  AGEA_check(type_rt, "Type is not defined!");
""")
        if type_obj.render_constructor:
          file_buffer.append(f"  type_rt->render_constructor = {type_obj.render_constructor};\n")
        if type_obj.render_destructor:
          file_buffer.append(f"  type_rt->render_destructor  = {type_obj.render_destructor};\n")
        file_buffer.append("}")

    file_buffer.append(f"""
  return true;
}}
bool
package::package_render_types_builder::destroy(::agea::core::static_package&)
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
