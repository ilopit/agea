import sys
import os


default_header = """// Smart Object Autogenerated Layout

// clang-format off

#include "include/model/caches/empty_objects_cache.h"
#include "include/model/caches/objects_cache.h"
#include "include/model/caches/caches_map.h"

#include "model/reflection/type_handlers/property_type_copy_handlers.h"
#include "model/reflection/type_handlers/property_type_serialization_handlers.h"
#include "model/reflection/type_handlers/property_type_serialization_update_handlers.h"
#include "model/reflection/type_handlers/property_type_serialization_handlers_custom.h"
#include "model/reflection/type_handlers/property_type_compare_handlers.h"
#include "model/reflection/lua_api.h"

#include <sol2_unofficial/sol.h>

using namespace agea;
using namespace agea::model;
"""

default_content = """
bool 
reflection::entry::set_up()
{
    property_type_copy_handlers::init();
    property_type_serialization_handlers::init();
    property_type_serialization_update_handlers::init();
    property_type_compare_handlers::init();
"""

default_footer = """
    object_reflection::fill_properties();
    return true;
}
// clang-format on
"""

methods_template = """
void
{1}::set_{2}({0} v)
{{
    m_{2} = v;
}}

{0}
{1}::get_{2}() const
{{
    return m_{2};
}}"""

methods_template_ref = """
void
{1}::set_{2}({0} v)
{{
    *m_{2} = v;
}}

{0}
{1}::get_{2}() const
{{
    return *m_{2};
}}"""

methods_template_decl = """
{2}:\\
{0} get_{1}() const;\\
{3}:\\
void set_{1}({0} v);\\"""

methods_template_decl_ref = """
{2}:\\
{0} get_{1}() const;\\
{3}:\\
void set_{1}({0} v);\\"""

soal_template = """
utils::id
{0}::META_type_id()
{{
    return AID("{0}");
}}

void 
{0}::META_class_set_type_id()
{{
    m_type_id = META_type_id();
}}

void 
{0}::META_class_set_architype_id()
{{
    m_architype_id = META_architype_id();
}}

reflection::object_reflection*
{0}::META_object_reflection()
{{
    static reflection::object_reflection rt{{
        std::is_same<this_class, base_class>::value
            ? nullptr
            : base_class::META_object_reflection(), META_type_id()}};
    return &rt;
}}

reflection::object_reflection*
{0}::reflection() const
{{
    return this_class::META_object_reflection();
}}

bool
{0}::META_construct(smart_object::construct_params& i)
{{
    /* Replace to dynamic cast */
    this_class::construct_params* cp = (this_class::construct_params*)&i;

    return construct(*cp);
}}

bool
{0}::META_post_construct()
{{
    return post_construct();
}}

std::shared_ptr<::smart_object>
{0}::META_create_empty_obj()
{{
    return this_class::META_class_create_empty_obj();
}}

std::shared_ptr<::{0}>
{0}::META_class_create_empty_obj()
{{
    auto s = std::make_shared<this_class>();
    s->META_class_set_type_id();
    s->META_class_set_architype_id();
    return s;
}}
"""

empty_template = """
{{
    using type = ::{0};
    ::agea::glob::empty_objects_cache::get()->insert(type::META_type_id(), type::META_class_create_empty_obj());
    auto dummy = type::META_object_reflection();
    (void)dummy;
}}
"""


property_template_start = """
{{
    using type       = ::{2};

    auto prop        = std::make_shared<property>();
    auto p           = prop.get();
    auto table       = type::META_object_reflection();

    table->m_properties.emplace_back(std::move(prop));

    // Main fields
    p->name                           = "{0}";
    p->offset                         = offsetof(type, m_{0});
    p->size                           = sizeof(type::m_{0});
    p->type                           = ::agea::reflection::type_resolver::resolve<decltype(type::m_{0})>();
    p->types_compare_handler          = property_type_compare_handlers::compare_handlers()[(size_t)p->type.type];\n
    // Extra fields
"""
property_template_end = """
}
"""

lua_binding_class_template = """
    {{
        static sol::usertype<{0}> lua_type = glob::lua_api::getr().state().new_usertype<{0}>(
        "{0}", sol::no_constructor,
            "i",
            [](const char* id) -> {0}*
            {{
                auto item = glob::objects_cache::get()->get_item(AID(id));

                if(!item)
                {{
                    return nullptr;
                }}

                return item->as<{0}>();
            }},
            "c",
            [](const char* id) -> {0}*
            {{
                auto item = glob::class_objects_cache::get()->get_item(AID(id));

                if(!item)
                {{
                    return nullptr;
                }}

                return item->as<{0}>();
            }}{1}
        );
"""

lua_binding_struct_template = """
    {{
        static sol::usertype<{0}> lua_type = glob::lua_api::getr().state().new_usertype<{0}>(
        "{0}", sol::constructors<{1}>()
        );
"""

lua_binding_template_end = """
    }"""

lua_binding_template_get_function = """
        lua_type["get_{0}"] = &{1}::get_{0};"""

lua_binding_template_set_function = """
        lua_type["set_{0}"] = &{1}::set_{0};"""


class file_context:
    def __init__(self):
        self.header = default_header
        self.includes = set()
        self.content = ""
        self.footer = default_footer
        self.soal = ""
        self.empty_cache = ""
        self.methods = ""
        self.lua_binding = ""
        self.lua_ctor = ""


class agea_class:
    def __init__(self):
        self.name = ""
        self.parent = ""
        self.properties = []
        self.functions = []


class agea_struct:
    def __init__(self):
        self.name = ""
        self.ctros = []
        self.functions = []


class agea_function:
    def __init__(self):
        self.name = ""


class agea_ctor:
    def __init__(self):
        self.name = ""


class agea_property:
    def __init__(self):
        self.name = ""
        self.category = ""
        self.type = ""
        self.access = "cpp_only"
        self.owner = ""
        self.hint = ""
        self.serializable = "no"
        self.property_ser_handler = ""
        self.property_des_handler = ""
        self.property_prototype_handler = ""
        self.property_compare_handler = ""
        self.property_copy_handler = ""
        self.gpu_data = ""
        self.copyable = "yes"
        self.updatable = "yes"
        self.ref = "false"
        self.has_default = "false"


def write_properties(context: file_context, prop: agea_property, current_class: agea_class):

    context.content += property_template_start.format(
        prop.name[2:], prop.type, prop.owner)

    if prop.access != "no":
        if prop.ref == "false":
            context.methods += methods_template.format(
                prop.type, str(prop.owner), prop.name[2:])
        else:
            context.methods += methods_template_ref.format(
                prop.type[:-1], str(prop.owner), prop.name[2:])

    if prop.access == "all" or prop.access == "read_only":
        context.lua_binding += lua_binding_template_get_function.format(
            prop.name[2:], current_class.name)

    if prop.access == "all":
        context.lua_binding += lua_binding_template_set_function.format(
            prop.name[2:], current_class.name)

    if prop.category != "":
        context.content += "    "
        context.content += 'p->category                       = "{0}";\n'.format(
            prop.category)

    if prop.gpu_data != "":
        context.content += "    "
        context.content += 'p->gpu_data                       = "{0}";\n'.format(
            prop.gpu_data)

    if prop.has_default == "true":
        context.content += "    "
        context.content += 'p->has_default                    = {0};\n'.format(prop.has_default)

    if prop.hint != "":
        context.content += "    "
        context.content += 'p->hints                          = {{{0}}};\n'.format(
            prop.hint)

    if prop.serializable == "true":
        context.content += "    "
        context.content += 'p->types_serialization_handler    = property_type_serialization_handlers::serializers()[(size_t)p->type.type];\n'
        context.content += "    "
        context.content += 'p->types_deserialization_handler  = property_type_serialization_handlers::deserializers()[(size_t)p->type.type];\n'

    if prop.property_ser_handler != "":
        context.content += "    "
        context.content += 'p->serialization_handler          = {0};\n'.format(
            prop.property_ser_handler)

    if prop.property_des_handler != "":
        context.content += "    "
        context.content += 'p->deserialization_handler        = {0};\n'.format(
            prop.property_des_handler)

    if prop.property_prototype_handler != "":
        context.content += "    "
        context.content += 'p->protorype_handler              = {0};\n'.format(
            prop.property_prototype_handler)

    if prop.property_compare_handler != "":
        context.content += "    "
        context.content += 'p->compare_handler                = {0};\n'.format(
            prop.property_compare_handler)

    if prop.property_copy_handler != "":
        context.content += "    "
        context.content += 'p->copy_handler                   = {0};\n'.format(
            prop.property_copy_handler)

    if prop.copyable != "no":
        context.content += "    "
        context.content += 'p->types_copy_handler             = property_type_copy_handlers::copy_handlers()[(size_t)p->type.type];\n'

    if prop.updatable != "no":
        context.content += "    "
        context.content += 'p->types_update_handler           = property_type_serialization_update_handlers::deserializers()[(size_t)p->type.type];\n'
    context.content += property_template_end


def setter_access_kw(w: str):
    return {"cpp_read_only": "protected", "cpp_only": "public", "read_only": "protected", "all": "public"}.get(w)


def getter_access_kw(w: str):
    return {"cpp_read_only": "public", "cpp_only": "public", "read_only": "public", "all": "public"}.get(w)


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def write_lua_class_type(context: file_context, current_class: agea_class):
    if current_class.parent == '':
        context.lua_binding += lua_binding_class_template.format(
            current_class.name, '')
    else:
        context.lua_binding += lua_binding_class_template.format(
            current_class.name, ',sol::base_classes, sol::bases<' + current_class.parent + '>()')


def write_lua_struct_type(context: file_context, current_struct: agea_struct):

    ctro_line = ''
    for c in current_struct.ctros:
        ctro_line += c.name
        ctro_line += ','

    context.lua_binding += lua_binding_struct_template.format(
        current_struct.name, ctro_line[:-1])


def process_file(original_file_full_path, original_file_rel_path, context: file_context):

    eprint("processing : {0} ...".format(original_file_full_path))

    class_name = os.path.basename(original_file_full_path)[:-2]

    cfg = open(original_file_full_path, 'r')
    lines = cfg.readlines()
    lines_count = len(lines)

    i = 0

    current_class = None
    current_struct = None

    while i != lines_count:
        line = lines[i].strip()

        if line.startswith("AGEA_class()"):

            if current_class is None:
                current_class = agea_class()

            i = i + 1
            final_tokens = []
            class_tokens = lines[i].replace(
                ":", " ").replace("\n", " ").replace(",", " ").split(" ")
            for t in class_tokens:
                if t not in {"class", "public", "private", str()}:
                    final_tokens.append(t)

            current_class.name = final_tokens[0]
            if len(final_tokens) > 1:
                current_class.parent = final_tokens[1]

        if line.startswith("AGEA_struct()"):

            if current_struct is None:
                current_struct = agea_struct()

            i = i + 1
            final_tokens = []
            class_tokens = lines[i].replace(
                ":", " ").replace("\n", " ").replace(",", " ").split(" ")
            for t in class_tokens:
                if t not in {"struct", "public", "private", str()}:
                    final_tokens.append(t)

            current_struct.name = final_tokens[0]

        if line.startswith("AGEA_function"):

            function_header_like = ""
            function_body_like = ""

            current_fucntion = agea_function()
            function_header_like += line + " "

            while i <= lines_count and lines[i].find(")") == -1:
                i = i + 1
                function_header_like += lines[i].strip() + " "

            i = i + 1
            function_body_like += lines[i] + " "

            while i <= lines_count and lines[i].find(")") == -1:
                i = i + 1
                function_body_like += lines[i].strip() + " "

            function_body_like = function_body_like.strip().replace("\n", " ")
            fff = function_body_like.split(" ")

            matches = next(x for x in fff if x.find("(") != -1)
            current_fucntion.name = matches[:matches.find("(")]
            if current_class:
                current_class.functions.append(current_fucntion)

            if current_struct:
                current_struct.functions.append(current_fucntion)

        if line.startswith("AGEA_property"):
            property_like = line + " "

            while i <= lines_count and lines[i].find(")") == -1:
                i = i + 1
                property_like += lines[i].strip() + " "

            i = i + 1
            field_tokens = lines[i].strip()[:-1].split()

            prop = agea_property()
            property_raw = property_like[property_like.find(
                "(") + 1:property_like.find(")")].split(", ")

            for pf in property_raw:
                pairs = pf.strip().split("=")
                eprint("DBG! {0}".format(pairs))
                pairs[0] = pairs[0][1:]
                pairs[1] = pairs[1][:-1]

                if len(pairs) != 2:
                    eprint("Wrong numbers of pairs! {0}".format(pairs))
                    exit(-1)

                if pairs[0] == "category":
                    prop.category = pairs[1]
                elif pairs[0] == "serializable":
                    prop.serializable = pairs[1]
                elif pairs[0] == "property_ser_handler":
                    prop.property_ser_handler = pairs[1]
                elif pairs[0] == "property_des_handler":
                    prop.property_des_handler = pairs[1]
                elif pairs[0] == "property_prototype_handler":
                    prop.property_prototype_handler = pairs[1]
                elif pairs[0] == "property_compare_handler":
                    prop.property_compare_handler = pairs[1]
                elif pairs[0] == "property_copy_handler":
                    prop.property_copy_handler = pairs[1]
                elif pairs[0] == "access":
                    prop.access = pairs[1]
                elif pairs[0] == "default":
                    prop.has_default = pairs[1]
                    if(len(field_tokens) < 3 or field_tokens[2] != "="):
                       eprint("Please provide default arument")
                       exit(-1)
                elif pairs[0] == "gpu_data":
                    prop.gpu_data = pairs[1]
                elif pairs[0] == "copyable":
                    prop.copyable = pairs[1]
                elif pairs[0] == "updatable":
                    prop.copyable = pairs[1]
                elif pairs[0] == "ref":
                    prop.ref = pairs[1]
                elif pairs[0] == "hint":
                    tokens = pairs[1].split(",")
                    prop.hint += ""
                    for t in tokens:
                        prop.hint += "\""
                        prop.hint += t
                        prop.hint += "\","
                    if len(prop.hint) > 0:
                        prop.hint = prop.hint[:-1]
                    prop.hint += ""
                else:
                    eprint("Unsupported property = " + pairs[0])
                    exit(-1)

            prop.type = field_tokens[0]
            prop.name = field_tokens[1]

            prop.owner = class_name
            current_class.properties.append(prop)

        if line.startswith("AGEA_ctor"):
            ctor_body_like = ""
            ctor = agea_ctor()
            ctor_header_like = line + " "

            while i <= lines_count and lines[i].find(")") == -1:
                i = i + 1
                ctor_header_like += lines[i].strip() + " "

            i = i + 1
            ctor_body_like += lines[i] + " "

            while i <= lines_count and lines[i].find(")") == -1:
                i = i + 1
                ctor_body_like += lines[i].strip() + " "

            ctor.name = ctor_body_like.strip().replace("\n", " ")
            current_struct.ctros.append(ctor)

        i = i + 1

    if current_class:
        context.soal += soal_template.format(class_name)
        context.empty_cache += empty_template.format(class_name)
        write_lua_class_type(context, current_class)

        for p in current_class.properties:
            write_properties(context, p, current_class)

        for p in current_class.functions:
            context.lua_binding += """       lua_type["{0}"] = &{1}::{0};
""".format(p.name, current_class.name)

        context.lua_binding += lua_binding_template_end

    if current_struct:
        write_lua_struct_type(context, current_struct)

        for p in current_struct.functions:
            context.lua_binding += """       lua_type["{0}"] = &{1}::{0};
""".format(p.name, current_struct.name)

        context.lua_binding += lua_binding_template_end

    include = '#include "' + original_file_rel_path + '"'

    context.includes.add(include)


def write_file(output_file, context):
    output = open(output_file, "w")
    output.write(context.header)
    output.write("\n\n")

    l = list(context.includes)
    l.sort()

    for i in l:
        output.write(i)
        output.write("\n")

    output.write("\n\n")
    output.write(context.soal)
    output.write("\n\n")
    output.write(default_content)
    output.write("\n\n")
    output.write(context.empty_cache)
    output.write("\n\n")
    output.write(context.content)
    output.write("\n\n")
    output.write(context.lua_binding)
    output.write("\n\n")
    output.write(context.footer)
    output.write("\n\n")
    output.write(context.methods)


def write_single_file(output_dir, file_path):
    class_name = os.path.basename(file_path)[:-2]

    full_file_path = os.path.basename(file_path)[:-2] + ".generated.h"

    header_file_content = f"""
#pragma once

#define AGEA_gen_meta__{class_name}()\\"""
    cfg = open(file_path, 'r')
    lines = cfg.readlines()
    lines_count = len(lines)
    i = 0
    while i != lines_count:
        line = lines[i].strip()

        property_like = ""

        # TODO, rewrite!
        if line.startswith("AGEA_property"):
            property_like += line + " "

            while i <= lines_count and lines[i].find(")") == -1:
                i = i + 1
                property_like += lines[i].strip() + " "

            i = i + 1
            field = lines[i].strip()[:-1].split()

            prop = agea_property()
            property_raw = property_like[property_like.find(
                "(") + 1:property_like.find(")")].split(", ")

            for pf in property_raw:
                pf = pf.strip()
                pairs = pf.split("=")
                pairs[0] = pairs[0][1:]
                pairs[1] = pairs[1][:-1]

                if len(pairs) != 2:
                    eprint("Wrong numbers of pairs! {0}".format(pairs))
                    exit(-1)

                if pairs[0] == "getter":
                    prop.getter = pairs[1]
                elif pairs[0] == "setter":
                    prop.setter = pairs[1]
                elif pairs[0] == "ref":
                    prop.ref = pairs[1]
                elif pairs[0] == "access":
                    prop.access = pairs[1]
                else:
                    eprint("Unknown property!")

            prop.type = field[0]
            prop.name = field[1]
            prop.owner = class_name

            if prop.access != "no":
                if prop.ref == "false":
                    header_file_content += methods_template_decl.format(
                        prop.type, prop.name[2:], getter_access_kw(prop.access), setter_access_kw(prop.access))
                else:
                    header_file_content += methods_template_decl_ref.format(
                        prop.type[:-1], prop.name[2:], getter_access_kw(prop.access), setter_access_kw(prop.access))

        i = i + 1
    header_file_content += "\nprivate:"

    full_file_path = os.path.join(output_dir, full_file_path)
    with open(full_file_path, 'w') as file:
        file.write(header_file_content)


def main(sol_cfg_path, root_dir, output_dir):

    print("SOLing : SOL config - {0}, root dir - {1}, output - {2}".format(
        sol_cfg_path, root_dir, output_dir))

    context = file_context()
    cfg = open(sol_cfg_path, 'r')
    lines = cfg.readlines()
    for f in lines:
        if len(f) > 0:
            file_path = os.path.join(root_dir, f[:-1]).replace("\\", "/")
            process_file(file_path, f[:-1], context)
            write_single_file(output_dir, file_path)

    output_file = output_dir + "/soal.generated.cpp"

    write_file(output_file, context)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3])
