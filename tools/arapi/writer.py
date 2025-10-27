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


def kind_to_string(kind: arapi.types.agea_type_kind):
  if kind == arapi.types.agea_type_kind.CLASS:
    return "agea_class"
  elif kind == arapi.types.agea_type_kind.STRUCT:
    return "agea_struct"
  elif kind == arapi.types.agea_type_kind.EXTERNAL:
    return "agea_external"
  else:
    exit(-1)


def write_ar_class_include_file(ar_type: arapi.types.agea_type, context: arapi.types.file_context,
                                output_dir):

  if ar_type.kind != arapi.types.agea_type_kind.CLASS:
    exit(-1)

  include_path = os.path.join(
      context.model_header_dir,
      os.path.basename(ar_type.name) + ".ar.h",
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


def generate_builder(should_generate: bool, name: str):
  if not should_generate:
    return ""

  return f"""struct package_{name}_builder : public ::agea::core::package_{name}_builder\\
{{                                                                                      \\
    public:                                                                             \\
        virtual bool build(::agea::core::static_package& sp) override;                  \\
        virtual bool destroy(::agea::core::static_package& sp) override;                \\
}};                                                                                     \\
"""


def write_ar_package_include_file(context: arapi.types.file_context, output_dir):

  include_path = os.path.join(context.package_header_dir, "package.ar.h")

  print("generating : " + include_path)

  with open(include_path, "w+") as ar_class_include:
    ar_class_include.write(f"""#pragma once\n\n""")

    ar_class_include.write(f"""
#define AGEA_gen_meta__{context.module_name}_package_model                        
#define AGEA_gen_meta__{context.module_name}_package_render
#define AGEA_gen_meta__{context.module_name}_package_builder

#if defined(AGEA_build__model)
#undef  AGEA_gen_meta__{context.module_name}_package_model
#define AGEA_gen_meta__{context.module_name}_package_model \\
public: \\
static void \\
reset_instance() \\
{{ \\
    s_instance.reset(); \\
}} \\
static void \\
init_instance() \\
{{ \\
    AGEA_check(!s_instance, "using on existed"); \\
    s_instance = std::make_unique<package>(); \\
}} \\
static package& \\
instance()\\
{{ \\
    AGEA_check(s_instance, "empty instance"); \\
    return *s_instance; \\
}} \\
{generate_builder(True, "types")}{generate_builder(True, "types_default_objects")} \\
private: \\
    static std::unique_ptr<package> s_instance;
#endif

#if defined(AGEA_build__render)
#undef  AGEA_gen_meta__{context.module_name}_package_render
#define AGEA_gen_meta__{context.module_name}_package_render \\
public: \\
{generate_builder(context.render_has_types_overrides,"render_types")}{generate_builder(context.render_has_custom_resources,"render_custom_resource")} \\
private: 
#endif

#if defined(AGEA_build__builder)
#undef  AGEA_gen_meta__{context.module_name}_package_builder
#define AGEA_gen_meta__{context.module_name}_package_builder \\

#endif
""")

    ar_class_include.write(f"""
#define AGEA_gen_meta__{context.module_name}_package \\
  AGEA_gen_meta__{context.module_name}_package_model \\
  AGEA_gen_meta__{context.module_name}_package_render \\
  AGEA_gen_meta__{context.module_name}_package_builder
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
        auto prop_rtype  = ::agea::glob::state::getr().get_rm()->get_type(property_td.type_id);

        auto prop        = std::make_shared<::agea::reflection::property>();
        auto p           = prop.get();

        {fc.module_name}_{t.name}_rt->m_properties.emplace_back(std::move(prop));

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
      ar_file.write(f"        p->serialization_handler  = {p.property_ser_handler};\n")

    if p.property_des_handler != "":
      ar_file.write(f"        p->deserialization_handler  = {p.property_des_handler};\n")

    if p.property_prototype_handler != "":
      ar_file.write(f"        p->protorype_handler  = {p.property_prototype_handler};\n")

    if p.property_compare_handler != "":
      ar_file.write(f"        p->compare_handler  = {p.property_compare_handler};\n")

    if p.property_copy_handler != "":
      ar_file.write(f"        p->copy_handler  = {p.property_copy_handler};\n")

    ar_file.write("    }\n")

  ar_file.write("    }\n")


def write_types_resolvers(fc: arapi.types.file_context):

  output_file = os.path.join(fc.package_header_dir, "types_resolvers.ar.h")
  with open(output_file, "w") as output:

    output.write(f"""#pragma once

#include <core/reflection/types.h>

#include <glue/type_ids.ar.h>

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
          value = ::agea::{t.id}
      }};
  }};
  """)

    output.write("}\n")


def write_lua_class_type(file, fc: arapi.types.file_context, t: arapi.types.agea_type):
  file.write(f"""
    {{
        *{t.name}_lua_type = ::agea::glob::state::getr().get_lua()->state().new_usertype<{t.get_full_type_name()}>(
        "{t.name}", sol::no_constructor,
            "i",
            [](const char* id) -> {t.get_full_type_name()}*
            {{
                auto item = ::agea::glob::state::getr().get_instance_objects_cache()->get_item(AID(id));

                if(!item)
                {{
                    return nullptr;
                }}

                return item->as<{t.get_full_type_name()}>();
            }},
            "c",
            [](const char* id) -> {t.get_full_type_name()}*
            {{
                auto item = ::agea::glob::state::getr().get_class_objects_cache()->get_item(AID(id));

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
             
    {t.name}__lua_script_extention<sol::usertype<{t.get_full_type_name()}>, {t.get_full_type_name()}>(*{t.name}_lua_type);

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
         *{t.name}_lua_type = ::agea::glob::state::getr().get_lua()->state().new_usertype<{t.get_full_type_name()}>(
        "{t.get_full_type_name()}", sol::constructors<{ctro_line[:-1]}>());
  """)


def write_lua_usertype_extention(fc: arapi.types.file_context):

  file_path = os.path.join(fc.package_header_dir, "types_script_importer.ar.h")

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


def model_generate_overrides_headers(fc: arapi.types.file_context):
  overrides = ""
  for include in fc.model_overrides:
    overrides += "#include <" + include + ">\n"

  return overrides


def write_object_model_reflection(package_ar_file, fc: arapi.types.file_context):

  with open(package_ar_file, "w+") as ar_file:

    model_conditional_header = model_generate_overrides_headers(fc)

    ar_file.write(f"""// Smart Object Autogenerated Reflection Layout

// clang-format off

#include "glue/type_ids.ar.h"

#include "packages/{fc.module_name}/package.{fc.module_name}.h"
#include "packages/{fc.module_name}/types_resolvers.ar.h"
#include "packages/{fc.module_name}/types_script_importer.ar.h"
{model_conditional_header}

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

std::unique_ptr<package> package::s_instance;

""")
    reflection_methods = ""
    for type in fc.types:
      ar_file.write(f"""
static std::unique_ptr<::agea::reflection::reflection_type> {fc.module_name}_{type.name}_rt;""")

      if type.kind != arapi.types.agea_type_kind.EXTERNAL or type.script_support:
        ar_file.write(f"""
static std::unique_ptr<sol::usertype<{type.get_full_type_name()}>> {type.name}_lua_type;""")

      if type.kind != arapi.types.agea_type_kind.CLASS:
        continue

      reflection_methods += f"""
const ::agea::reflection::reflection_type& 
{type.name}::AR_TYPE_reflection()
{{
    return *{fc.module_name}_{type.name}_rt;
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
AGEA_gen__static_schedule(::agea::core::state::state_stage::create,
    [](agea::core::state& s)
    {{
      package::init_instance();
      package::instance().register_package_extention<package::package_types_builder>(); 
    }});
                  
AGEA_gen__static_schedule(::agea::core::state::state_stage::connect,
    [](agea::core::state& s)
    {{
      s.get_pm()->register_static_package({fc.module_name}::package::instance());
    }});

bool
package::package_types_builder::build(static_package& sp)
{{
    auto pkg = &::agea::{fc.module_name}::package::instance();
""")

    for t in fc.types:
      ar_file.write(f"""
{{      
    const int type_id = ::agea::reflection::type_resolver<{t.get_full_type_name()}>::value;
    AGEA_check(type_id != -1, "Type is not defined!");
    {fc.module_name}_{t.name}_rt = std::make_unique<::agea::reflection::reflection_type>(type_id, AID("{t.name}"));
    auto& rt         = *add(sp, {fc.module_name}_{t.name}_rt.get());
    rt.type_id       = type_id;
    rt.type_class    = ::agea::reflection::reflection_type::reflection_type_class::{kind_to_string(t.kind)};

    rt.module_id     = AID("{fc.module_name}");
    rt.size          = sizeof({t.get_full_type_name()});
""")
      if t.kind != arapi.types.agea_type_kind.EXTERNAL or t.script_support:
        ar_file.write(f"""    {t.name}_lua_type = std::make_unique<sol::usertype<{t.get_full_type_name()}>>();\n""")

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

    auto parent_rt =  ::agea::glob::state::getr().get_rm()->get_type(parent_type_id);
    AGEA_check(parent_rt, "Type is not defined!");

    rt.parent = parent_rt;
""")

      if t.compare_handler:
        ar_file.write(f"    rt.compare                = {t.compare_handler};\n")

      if t.copy_handler:
        ar_file.write(f"    rt.copy                   = {t.copy_handler};\n")

      if t.serialize_handler:
        ar_file.write(f"    rt.serialize              = {t.serialize_handler};\n")

      if t.deserialize_handler:
        ar_file.write(f"    rt.deserialize            = {t.deserialize_handler};\n")

      if t.deserialize_from_proto_handle:
        ar_file.write(f"    rt.deserialize_with_proto = {t.deserialize_from_proto_handle};\n")

      if t.to_string_handle:
        ar_file.write(f"    rt.to_string              = {t.to_string_handle};\n")

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

    ar_file.write("\n    return true;\n}\n\n")

    ar_file.write(f"""
bool
package::package_types_builder::destroy(static_package& sp)
{{
""")
    for t in reversed(fc.types):
      ar_file.write(f"""  {fc.module_name}_{t.name}_rt.reset();\n""")

      if t.kind != arapi.types.agea_type_kind.EXTERNAL or t.script_support:
        ar_file.write(f"""  {t.name}_lua_type.reset();\n""")

      if t.kind != arapi.types.agea_type_kind.CLASS:
        continue

    ar_file.write(f"""   return true;
}}
bool
package::package_types_default_objects_builder::build(static_package& sp)
{{
    auto pkg = &::agea::{fc.module_name}::package::instance();
""")
    for t in fc.types:
      if t.kind == arapi.types.agea_type_kind.CLASS:
        ar_file.write(f"    pkg->create_default_class_obj<{t.get_full_type_name()}>();\n")

    ar_file.write("\n    return true;\n}\n\n")

    ar_file.write(f"""
bool
package::package_types_default_objects_builder::destroy(static_package& sp)
{{
    auto pkg = &::agea::{fc.module_name}::package::instance();
""")
    for t in fc.types:
      if t.kind == arapi.types.agea_type_kind.CLASS:
        ar_file.write(f"    pkg->destroy_default_class_obj<{t.get_full_type_name()}>();\n")

    ar_file.write("\n    return true;\n}\n\n")



    ar_file.write(fc.properies_access_methods)

    ar_file.write("\n}\n")

  write_types_resolvers(fc)

  write_lua_usertype_extention(fc)


def write_render_types_reflection(package_ar_file, fc: arapi.types.file_context):

  with open(package_ar_file, "w+") as ar_file:

    render_overrides = ""
    for include in fc.render_overrides:
      render_overrides += "#include <" + include + ">\n"

    ar_file.write(f"""// Smart Object Autogenerated Reflection Layout

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
#include <core/global_state.h>
#include <glue/type_ids.ar.h>

namespace agea::{fc.module_name} {{
""")

    if fc.render_has_custom_resources:
      ar_file.write("""
AGEA_gen__static_schedule(::agea::core::state::state_stage::connect,
    [](::agea::core::state& s)
    {{
      package::instance().register_package_extention<package::package_render_custom_resource_builder>(); 
    }});
""")

    if fc.render_has_types_overrides:
      ar_file.write("""
AGEA_gen__static_schedule(::agea::core::state::state_stage::connect,
    [](::agea::core::state& s)
    {{
      package::instance().register_package_extention<package::package_render_types_builder>(); 
    }});
""")

    if fc.render_has_types_overrides:

      ar_file.write(f"""
bool
package::package_render_types_builder::build(::agea::core::static_package& sp)
{{
  auto pkg = &::agea::{fc.module_name}::package::instance();
""")
      for t in fc.types:
        if t.kind == arapi.types.agea_type_kind.CLASS and (t.render_constructor
                                                           or t.render_destructor):

          ar_file.write(f"""
{{                  
  auto type_rt =  ::agea::glob::state::getr().get_rm()->get_type(::agea::{t.id});
  AGEA_check(type_rt, "Type is not defined!");
""")
          if t.render_constructor:
            ar_file.write(f"""  type_rt->render_constructor = {t.render_constructor};
""")
          if t.render_destructor:
            ar_file.write(f"""  type_rt->render_destructor  = {t.render_destructor};\n""")
          ar_file.write("}")

      ar_file.write(f"""
  return true;
}}
                  
}}""")
