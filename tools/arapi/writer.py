import os
import arapi.types

should_have_getter = {"cpp_readonly", "cpp_only", "script_readonly", "read_only", "all"}

should_have_setter = {"cpp_writeonly", "cpp_only", "script_writeonly", "write_only", "all"}

lua_binding_struct_template = """   {{
        static sol::usertype<{type}> lua_type = ::agea::glob::lua_api::getr().state().new_usertype<{type}>(
        "{type}", sol::constructors<{ctor_line}>());
"""

lua_binding_template_end = """
}"""


def write_ar_class_include_file(ar_type: arapi.types.agea_type, context: arapi.types.file_context,
                                output_dir):

  if ar_type.kind != arapi.types.agea_type_kind.CLASS:
    exit(-1)

  include_path = os.path.join(
      output_dir,
      "packages",
      context.module_name,
      os.path.basename(ar_type.name) + ".generated.h",
  )

  print("generating : " + include_path)

  with open(include_path, "w+") as ar_class_include:
    ar_class_include.write(f"""#pragma once

#define AGEA_gen_meta__{ar_type.name}()   \\
    friend class package; \\
""")

    for p in ar_type.properties:
      if p.access in should_have_getter:
        ar_class_include.write(f"""public:\\
{p.type} get_{p.name_cut}() const;\\
private:\\
""")

      if p.access in should_have_setter:
        ar_class_include.write(f"""public:\\
              void set_{p.name_cut}({p.type} v);\\
              private:\\
              """)


def write_property_access_methods(fc: arapi.types.file_context, p: arapi.types.agea_property):

  if p.access in should_have_getter:
    fc.properies_access_methods += f"""{p.type}
{p.owner}::get_{p.name_cut}() const
{{
    return m_{p.name_cut};
}}

"""

  if p.access in should_have_setter:
    fc.properies_access_methods += f"""void
{p.owner}::set_{p.name_cut}({p.type} v)
{{
"""
    if p.check_not_same:
      fc.properies_access_methods += "    if(m_{property} == v) {{ return; }}\n".format(
          property=p.name_cut)

    fc.properies_access_methods += "    m_{property} = v;\n".format(property=p.name_cut)

    if p.invalidates_transform:
      fc.properies_access_methods += "    mark_transform_dirty();\n    update_children_matrixes();\n"

    if p.invalidates_render:
      fc.properies_access_methods += "    mark_render_dirty();\n"

    fc.properies_access_methods += "}\n\n"


def write_properties(ar_file, fc: arapi.types.file_context, t: arapi.types.agea_type):

  if len(t.properties) == 0:
    return

  ar_file.write("    {\n")

  for p in t.properties:

    write_property_access_methods(fc, p)

    ar_file.write(f"""
    {{
        using type       = {p.owner};

        auto property_td = ::agea::reflection::agea_type_resolve<decltype(type::m_{p.name_cut})>();
        auto prop_rtype  = ::agea::glob::reflection_type_registry::getr().get_type(property_td.type_id);

        auto prop        = std::make_shared<::agea::reflection::property>();
        auto p           = prop.get();

        {fc.module_name}_{t.name}_rt.m_properties.emplace_back(std::move(prop));

        // Main fields
        p->name                           = "{p.name_cut}";
        p->offset                         = offsetof(type, m_{p.name_cut});
        p->rtype                          = prop_rtype;
""")

    if p.category != "":
      ar_file.write(f"        p->category  = \"{p.category}\";\n")

    if p.gpu_data != "":
      ar_file.write(f"        p->gpu_data  = \"{p.gpu_data}\";\n")

    if p.has_default == "true":
      ar_file.write(f"        p->has_default  = {p.has_default};\n")

    if p.serializable == "true":
      ar_file.write(f"        p->serializable  = true;\n")

    if p.invalidates_render:
      ar_file.write(
          f"        p->render_subobject  = std::is_base_of_v<::agea::root::smart_object, typename std::remove_pointer_t<{p.type}>>;\n"
      )

    if p.property_ser_handler != "":
      ar_file.write(
          f"        p->serialization_handler  = ::agea::reflection::{p.property_ser_handler};\n")

    if p.property_des_handler != "":
      ar_file.write(
          f"        p->deserialization_handler  = ::agea::reflection::{p.property_des_handler};\n")

    if p.property_prototype_handler != "":
      ar_file.write(
          f"        p->protorype_handler  = ::agea::reflection::{p.property_prototype_handler};\n")

    if p.property_compare_handler != "":
      ar_file.write(
          f"        p->compare_handler  = ::agea::reflection::{p.property_compare_handler};\n")

    if p.property_copy_handler != "":
      ar_file.write(f"        p->copy_handler  = ::agea::reflection::{p.property_copy_handler};\n")

    ar_file.write("    }\n")

  ar_file.write("    }\n")


def write_package_ids_include(fc: arapi.types.file_context):

  include_path = os.path.join(fc.output_dir, "packages", fc.module_name, "types_ids.ar.h")

  with open(include_path, "w+") as include_file:

    include_file.write(f"""#pragma once

#include "packages/{fc.module_name}/types_meta_ids.ar.h"

namespace {fc.full_module_name} {{
   enum {{
""")

    for t in fc.types:

      include_file.write(f"            {t.id},\n")

    include_file.write(f"            {fc.module_name}__count\n }};\n }}\n")


def write_types_resolvers(fc: arapi.types.file_context):

  output_file = os.path.join(fc.output_dir, "packages", fc.module_name, "types_resolvers.ar.h")
  with open(output_file, "w") as output:

    output.write(f"""#pragma once

#include "core/reflection/types.h"

#include "packages/{fc.module_name}/types_ids.ar.h"

""")

    l = list(fc.includes)
    l.sort()

    for i in l:
      output.write(i)
      output.write("\n")

    ns = """
  namespace agea::reflection
  {
  """
    output.write(ns)

    for t in fc.types:
      tt = ''
      if not t.built_in:
        tt = '::' + t.full_name
      else:
        tt = t.full_name

      output.write(f"""
  template <>
  struct type_resolver<{t.get_full_type_name()}>
  {{
      enum
      {{
          value = {fc.full_module_name}::{t.id}
      }};
  }};
  """)

    output.write("}\n")


def write_lua_class_type(file, fc: arapi.types.file_context, t: arapi.types.agea_type):
  file.write(f"""
    {{
        {t.name}_lua_type = ::agea::glob::lua_api::getr().state().new_usertype<{t.get_full_type_name()}>(
        "{t.name}", sol::no_constructor,
            "i",
            [](const char* id) -> {t.get_full_type_name()}*
            {{
                auto item = ::agea::glob::objects_cache::get()->get_item(AID(id));

                if(!item)
                {{
                    return nullptr;
                }}

                return item->as<{t.get_full_type_name()}>();
            }},
            "c",
            [](const char* id) -> {t.get_full_type_name()}*
            {{
                auto item = ::agea::glob::proto_objects_cache::get()->get_item(AID(id));

                if(!item)
                {{
                    return nullptr;
                }}

                return item->as<{t.get_full_type_name()}>();
            }}""")

  if t.parent_type:
    file.write(f""",sol::base_classes, sol::bases<{t.parent_type.get_full_type_name()}>()
               """)
    file.write(f""");
             
    {t.name}__lua_script_extention<sol::usertype<{t.get_full_type_name()}>, {t.get_full_type_name()}>({t.name}_lua_type);

}}""")
  else:
    file.write(f""");
    }}""")


def write_lua_struct_type(file, fc: arapi.types.file_context, t: arapi.types.agea_type):
  ctro_line = ""
  for c in t.ctros:
    ctro_line += c.name
    ctro_line += ","

  file.write(f"""
         {t.name}_lua_type = ::agea::glob::lua_api::getr().state().new_usertype<{t.get_full_type_name()}>(
        "{t.get_full_type_name()}", sol::constructors<{ctro_line[:-1]}>());
  """)


def write_lua_usertype_extention(fc: arapi.types.file_context):

  file_path = os.path.join(fc.output_dir, "packages", fc.module_name, "types_script_importer.ar.h")

  with open(file_path, "w") as file:
    file.write("""#pragma once

""")

    for c in fc.types:

      file.write(f"""template <typename T, typename K>
void
{c.name}__lua_script_extention(T& lua_type)
{{
""")
      if c.parent_type:
        file.write("    " + c.parent_type.name + "__lua_script_extention<T, K>(lua_type);\n")

      for p in c.properties:
        if p.access == "all" or p.access == "read_only":
          file.write(f"""    lua_type["get_{p.name_cut}"] = &K::get_{p.name_cut};
""")

        if p.access == "all" or p.access == "write_only":
          file.write(f"""    lua_type["set_{p.name_cut}"] = &K::set_{p.name_cut};
""")

      for p in c.functions:
        file.write(f"""    lua_type["{p.name}"] = &K::{p.name};
""")

      file.write(f"""}}

""")


def write_module_reflection(package_ar_file, fc: arapi.types.file_context):

  with open(package_ar_file, "w+") as ar_file:

    ar_file.write(f"""// Smart Object Autogenerated Reflection Layout

// clang-format off

#include "packages/{fc.module_name}/package.{fc.module_name}.h"
#include "packages/{fc.module_name}/types_ids.ar.h"
#include "packages/{fc.module_name}/types_resolvers.ar.h"
#include "packages/{fc.module_name}/types_script_importer.ar.h"

#include "packages/{fc.module_name}/properties_custom.h"

#include <core/caches/caches_map.h>
#include <core/caches/objects_cache.h>
#include <core/reflection/reflection_type.h>
#include <core/reflection/reflection_type_utils.h>
#include <core/reflection/lua_api.h>
#include <core/object_constructor.h>
#include <core/package_manager.h>
#include <core/package.h>



#include <utils/static_initializer.h>

#include <sol2_unofficial/sol.h>

namespace agea::{fc.module_name} {{

package&
package::instance()
{{
    static package s_instance;
    return s_instance;
}}

""")
    reflection_methods = ""
    for type in fc.types:
      ar_file.write(f"""
static ::agea::reflection::reflection_type {fc.module_name}_{type.name}_rt;""")

      if type.kind != arapi.types.agea_type_kind.EXTERNAL or type.script_support:
        ar_file.write(f"""
static sol::usertype<{type.get_full_type_name()}> {type.name}_lua_type;""")

      if type.kind != arapi.types.agea_type_kind.CLASS:
        continue

      reflection_methods += f"""
const ::agea::reflection::reflection_type& 
{type.name}::AR_TYPE_reflection()
{{
    return {fc.module_name}_{type.name}_rt;
}}                  

std::shared_ptr<::agea::root::smart_object> 
{type.name}::AR_TYPE_create_empty_gen_obj(const ::agea::utils::id& id)
{{    
    return {type.name}::AR_TYPE_create_empty_obj(id);
}}

std::shared_ptr<{type.name}>
{type.name}::AR_TYPE_create_empty_obj(const ::agea::utils::id& id)
{{
    auto s = std::make_shared<this_class>();
    s->META_set_reflection_type(&this_class::AR_TYPE_reflection());
    s->META_set_id(id);
    return s;
}}

std::unique_ptr<::agea::root::base_construct_params>
{type.name}::AR_TYPE_create_gen_default_cparams()
{{
    auto ptr = std::make_unique<{type.name}::construct_params>();          

    return ptr;
}}

::agea::utils::id
{type.name}::AR_TYPE_id()
{{
    return AID("{type.name}");
}}

bool
{type.name}::META_construct(const ::agea::root::base_construct_params& i)
{{
    /* Replace to dynamic cast */
    auto p = (this_class::construct_params*)&i;

    return construct(*p);
}}
"""

    ar_file.write("\n\n")
    ar_file.write(reflection_methods)

    ar_file.write(f"""
AGEA_schedule_static_init(
    []()
    {{ package::instance().register_package_extention<package::package_types_loader>(); }});

bool
package::package_types_loader::load(static_package& sp)
{{
    auto pkg = &::agea::{fc.module_name}::package::instance();
""")

    for t in fc.types:
      ar_file.write(f"""
{{      
    const int type_id = ::agea::reflection::type_resolver<{t.get_full_type_name()}>::value;
    AGEA_check(type_id != -1, "Type is not defined!");

    auto& rt         = {fc.module_name}_{t.name}_rt; 
    rt.type_id       = type_id;
    rt.type_name     = AID("{t.name}");

    
    ::agea::glob::reflection_type_registry::getr().add_type(&rt);

    rt.module_id     = AID("{fc.module_name}");
    rt.size          = sizeof({t.get_full_type_name()});
""")

      if t.kind == arapi.types.agea_type_kind.CLASS:
        ar_file.write(f"""
    rt.alloc         = {t.name}::AR_TYPE_create_empty_gen_obj;
    rt.cparams_alloc = {t.name}::AR_TYPE_create_gen_default_cparams;
""")

      if t.architype:
        ar_file.write(f"    rt.arch         = core::architype::{t.architype};\n")

      if t.parent_type:
        ar_file.write(f"""
    int parent_type_id = ::agea::reflection::type_resolver<{ t.parent_type.name}>::value;
    AGEA_check(parent_type_id != -1, "Type is not defined!");

    auto parent_rt = ::agea::glob::reflection_type_registry::getr().get_type(parent_type_id);
    AGEA_check(parent_rt, "Type is not defined!");

    rt.parent = parent_rt;
""")

      ar_file.write(f"    rt.inherit();\n")

      if t.compare_handler or t.default_handlers:
        h = t.compare_handler if t.compare_handler else f'::agea::reflection::utils::default_compare<{t.get_full_type_name()}>'
        ar_file.write(f"    rt.compare = {h};\n")

      if t.copy_handler or t.default_handlers:
        h = t.copy_handler if t.copy_handler else f'::agea::reflection::utils::default_copy<{t.get_full_type_name()}>'
        ar_file.write(f"    rt.copy = {h};\n")

      if t.serialize_handler or t.default_handlers:
        h = t.serialize_handler if t.serialize_handler else f'::agea::reflection::utils::default_serialize<{t.get_full_type_name()}>'
        ar_file.write(f"    rt.serialize = {h};\n")

      if t.deserialize_handler or t.default_handlers:
        h = t.deserialize_handler if t.deserialize_handler else f'::agea::reflection::utils::default_deserialize<{t.get_full_type_name()}>'
        ar_file.write(f"    rt.deserialize = {h};\n")

      if t.deserialize_from_proto_handle or t.default_handlers:
        h = t.deserialize_from_proto_handle if t.deserialize_from_proto_handle else f'::agea::reflection::utils::default_deserialize_from_proto<{t.get_full_type_name()}>'
        ar_file.write(f"    rt.deserialize_with_proto = {h};\n")

      if t.to_string_handle or t.default_handlers:
        h = t.to_string_handle if t.to_string_handle else f'::agea::reflection::utils::default_to_string<{t.get_full_type_name()}>'
        ar_file.write(f"    rt.to_string = {h};\n")

      ar_file.write("}\n")

    for t in fc.types:
      if type.kind == arapi.types.agea_type_kind.CLASS:
        write_properties(ar_file, fc, t)

    for t in fc.types:
      if t.kind == arapi.types.agea_type_kind.CLASS:
        write_lua_class_type(ar_file, fc, t)
      elif t.kind == arapi.types.agea_type_kind.STRUCT:
        write_lua_struct_type(ar_file, fc, t)

    ar_file.write("\n")

    for t in fc.types:
      if t.kind == arapi.types.agea_type_kind.CLASS:
        ar_file.write(f"    pkg->register_type<{t.get_full_type_name()}>();\n")

    ar_file.write("\n    return true;\n}\n\n")

    ar_file.write(fc.properies_access_methods)

    ar_file.write("\n}\n")

  write_package_ids_include(fc)

  write_types_resolvers(fc)

  write_lua_usertype_extention(fc)
