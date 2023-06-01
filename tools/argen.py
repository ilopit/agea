import sys
import os
import operator
import json
from collections import deque

default_header = """// Smart Object Autogenerated Layout

// clang-format off

#include "core/caches/caches_map.h"
#include "core/caches/objects_cache.h"
#include "core/reflection/lua_api.h"
#include "core/object_constructor.h"
#include "core/package_manager.h"
#include "core/package.h"

#include "{module_name}/{module_name}_module.h"
#include "{module_name}/{module_name}_types_ids.ar.h"
#include "{module_name}/{module_name}_types_resolvers.ar.h"
{custom_include}

#include <sol2_unofficial/sol.h>

"""

default_content = """
bool
{module_name}_module::init_reflection()
{{
    auto pkg = std::make_unique<agea::core::package>(AID("{module_name}"), agea::core::package_type::type);

"""

default_footer = """
    ::agea::glob::package_manager::getr().register_package(pkg);

    return true;
}}
// clang-format on
"""

modules_instance_template = """

{module_name}_module&
{module_name}_module::instance()
{{
    static {module_name}_module s_module(AID("{module_name}"));
    return s_module;
}}"""

methods_template = """
void
{type}::set_{property}({property_type} v)
{{
    m_{property} = v;
}}

{property_type}
{type}::get_{property}() const
{{
    return m_{property};
}}"""

soal_template = """

const ::agea::reflection::reflection_type& 
{type}::AR_TYPE_reflection()
{{
    return {module_name}_{type}_rt;
}}                  

std::shared_ptr<::agea::root::smart_object> 
{type}::AR_TYPE_create_empty_gen_obj(const ::agea::utils::id& id)
{{    
    return {type}::AR_TYPE_create_empty_obj(id);
}}

std::shared_ptr<{type}>
{type}::AR_TYPE_create_empty_obj(const ::agea::utils::id& id)
{{
    auto s = std::make_shared<this_class>();
    s->META_set_reflection_type(&this_class::AR_TYPE_reflection());
    s->META_set_id(id);
    return s;
}}

::agea::utils::id
{type}::AR_TYPE_id()
{{
    return AID("{type}");
}}

bool
{type}::META_construct(const ::agea::root::smart_object::construct_params& i)
{{
    /* Replace to dynamic cast */
    this_class::construct_params* cp = (this_class::construct_params*)&i;

    return construct(*cp);
}}
"""


empty_template_with_parent = """
{{
    int type_id = ::agea::reflection::type_resolver<{type}>::value;
    AGEA_check(type_id != -1, "Type is not defined!");

    auto type = ::agea::glob::reflection_type_registry::getr().get_type(type_id);
    AGEA_check(type, "Type is not defined!");

    int parent_type_id = ::agea::reflection::type_resolver<{parent}>::value;
    AGEA_check(parent_type_id != -1, "Type is not defined!");

    auto parent = ::agea::glob::reflection_type_registry::getr().get_type(parent_type_id);
    AGEA_check(parent, "Type is not defined!");

    type->parent = parent;
}}
"""

empty_template = """
{{
    const int type_id = ::agea::reflection::type_resolver<{type}>::value;
    AGEA_check(type_id != -1, "Type is not defined!");

    auto& rt        = {module_name}_{short_type}_rt;
    rt.type_id      = type_id;
    rt.type_name    = AID("{short_type}");

    
    ::agea::glob::reflection_type_registry::getr().add_type(&rt);

    rt.module_id    = AID("{module_name}");
    rt.size         = sizeof({type});
    rt.alloc        = {type}::AR_TYPE_create_empty_gen_obj;
"""

empty_template_no_class = """
{{
    const int type_id = ::agea::reflection::type_resolver<{type}>::value;
    AGEA_check(type_id != -1, "Type is not defined!");

    auto& rt        = {module_name}_{short_type}_rt;
    rt.type_id      = type_id;
    rt.type_name    = AID("{short_type}");

    ::agea::glob::reflection_type_registry::getr().add_type(&rt);

    rt.module_id    = AID("{module_name}");
    rt.size         = sizeof({type});
"""

smart_object_reg_package_type = """    pkg->register_type<{type}>();
"""


type_template = """
{{
    ::agea::reflection::reflection_type rt;
    rt.type_id   = type_id;
    rt.module_id = AID("{module_name}");
    rt.size = sizeof({type});

    ::agea::glob::reflection_type_registry::getr().add_type(std::move(rt));

"""

type_resolver = """
template <>
struct type_resolver<{full_type}>
{{
    enum
    {{
        value = ::{full_module_name}::{module_name}__{type}
    }};
}};
"""


property_template_start = """
{{
    using type       = {type};

    auto td          = ::agea::reflection::agea_type_resolve<{type}>();
    auto property_td = ::agea::reflection::agea_type_resolve<decltype(type::m_{property})>();

    auto rtype       = ::agea::glob::reflection_type_registry::getr().get_type(td.type_id);
    auto prop_rtype  = ::agea::glob::reflection_type_registry::getr().get_type(property_td.type_id);

    auto prop        = std::make_shared<::agea::reflection::property>();
    auto p           = prop.get();

    rtype->m_properties.emplace_back(std::move(prop));

    // Main fields
    p->name                           = "{property}";
    p->offset                         = offsetof(type, m_{property});
    p->rtype                          = prop_rtype;
    // Extra fields
"""
property_template_end = """
}
"""

lua_binding_class_template = """
    {{
        static sol::usertype<{type}> lua_type = ::agea::glob::lua_api::getr().state().new_usertype<{type}>(
        "{type}", sol::no_constructor,
            "i",
            [](const char* id) -> {type}*
            {{
                auto item = ::agea::glob::objects_cache::get()->get_item(AID(id));

                if(!item)
                {{
                    return nullptr;
                }}

                return item->as<{type}>();
            }},
            "c",
            [](const char* id) -> {type}*
            {{
                auto item = ::agea::glob::proto_objects_cache::get()->get_item(AID(id));

                if(!item)
                {{
                    return nullptr;
                }}

                return item->as<{type}>();
            }}{lua_end}
        );
"""

lua_binding_struct_template = """
    {{
        static sol::usertype<{type}> lua_type = ::agea::glob::lua_api::getr().state().new_usertype<{type}>(
        "{type}", sol::constructors<{ctor_line}>()
        );
"""

lua_binding_template_end = """
    }"""

lua_binding_template_get_function = """
        lua_type["get_{property}"] = &{type}::get_{property};"""

lua_binding_template_set_function = """
        lua_type["set_{property}"] = &{type}::set_{property};"""


gen_getter = {"cpp_readonly", "cpp_only",
              "script_readonly", "read_only", "all"}

gen_setter = {"cpp_writeonly", "cpp_only",
              "script_writeonly", "write_only", "all"}


def extstrip(value: str):
    removal_list = [' ', '\t', '\n', '\r']
    for s in removal_list:
        value = value.replace(s, '')
    return value


def parse_attributes(name: str, lines, index, max_index):

    property_like = lines[index]
    while index <= max_index and lines[index].find(")") == -1:
        index = index + 1
        property_like += lines[index].strip() + " "

    index = index + 1
    properties = property_like[property_like.find(
        "(") + 1:property_like.find(")")].split(", ")

    properties[:] = [x for x in properties if x]

    return index, properties


class file_context:
    def __init__(self, module_name, module_namespace):
        self.module_name = module_name
        if module_namespace:
            self.full_module_name = module_namespace + "::" + module_name
        else:
            self.full_module_name = module_name

        self.includes = set()
        self.types = list()
        self.custom_types = list()
        self.content = ""
        self.footer = default_footer.format(
            full_module_name=self.full_module_name)
        self.soal = ""
        self.empty_cache = ""
        self.reg_types = ""
        self.parent = ""
        self.methods = ""
        self.lua_binding = ""
        self.lua_ctor = ""
        self.has_custom_types = False
        self.has_custom_properties = False
        self.classes = []
        self.structs = []


class agea_class:
    def __init__(self):
        self.name = ""
        self.parent = ""
        self.properties: list[agea_property] = []
        self.functions = []
        self.architype = ""


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


module_register_template = """#include "engine/active_modules.h"

namespace agea
{{
namespace engine
{{
void
register_modules()
{{
{modules}
}}
}}  // namespace engine
}}  // namespace agea
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
        self.active_module_path = os.path.join(
            a, "engine", "active_modules.cpp")
        self.active_module_include_path = os.path.join(
            a, "engine",  "active_modules.h")
        self.handled = {}
        self.graph = {}
        self.order = deque()

    def load(self):

        root = agea_range("0000", 0, 0)
        root.has_render = True
        self.ranges.append(root)
        if os.path.exists(self.path):
            with open(self.path, 'r') as file:
                lines = file.readlines()
                for l in lines:
                    tokens = l.strip().split(":")

                    if tokens[0] == "root":
                        self.ranges[0].offset = 0
                        self.ranges[0].count = int(tokens[2])
                    else:

                        self.ranges.append(agea_range(
                            tokens[0], tokens[1], tokens[2]))

                        self.read_config(self.ranges[-1])

            self.ranges.sort(key=operator.attrgetter('module'))

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

    def add(self, module_name,  count):

        r = agea_range(module_name, 0, count)

        self.read_config(r)

        self.ranges.append(r)
        self.ranges.sort(key=operator.attrgetter('module'))

    def update(self, module_name, types_count):
        rearrange = 0
        if module_name == "root":
            if types_count > self.ranges[0].count:
                self.ranges[0].count = ((types_count//16) + 1) * 16
                offset = self.ranges[0].count
        else:
            rearrange = self.find(module_name)
            if rearrange == -1:
                self.add(module_name, ((types_count//16) + 1) * 16)
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

        mod_include_template = """#include "{0}/{0}_module.h"
"""
        mod_register_template = """    glob::module_manager::getr().register_module<{0}::{0}_module>();
"""

        mod_render_include_template = """#include "{0}/render/{0}_module_render_bridge.h"
"""
        mod_render_register_template = """    glob::module_manager::getr().register_module<{0}::{0}_module_render_bridge>();
"""

        with open(self.path, 'w') as file:

            file.write(range_desc.format(
                module_name="root", offset=self.ranges[0].offset, count=self.ranges[0].count))

            file.write('\n')

            for r in range(1, len(self.ranges)):
                r = self.ranges[r]
                file.write(range_desc.format(
                    module_name=r.module, offset=r.offset, count=r.count))

                file.write('\n')

        self.gen_register()

        mod_includes = "#pragma once\n"
        mod_register = ""

        for m in self.order:

            mod_includes += mod_include_template.format(m.module)
            mod_register += mod_register_template.format(m.module)

            if m.has_render:
                mod_includes += mod_render_include_template.format(
                    m.module)
                mod_register += mod_render_register_template.format(
                    m.module)

        with open(self.active_module_path, 'w') as file:
            file.write(module_register_template.format(modules=mod_register))

        mod_includes += """
namespace agea
{
namespace engine
{
void
register_modules();
}
}
"""

        with open(self.active_module_include_path, 'w') as file:
            file.write(mod_includes)


class agea_property:
    def __init__(self):
        self.name = ""
        self.name_cut = ""
        self.category = ""
        self.type = ""
        self.access = "no"
        self.owner = ""
        self.hint = ""
        self.serializable = "false"
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


access_setter_impl = """void
{type}::set_{property}({property_type} v)
{{
    m_{property} = v;
}}
"""
access_getter_impl = """{property_type}
{type}::get_{property}() const
{{
    return m_{property};
}}
"""


def write_properties(context: file_context, prop: agea_property, current_class: agea_class):

    context.content += property_template_start.format(module_name=context.module_name,
                                                      property=prop.name_cut, property_type=prop.type, type=prop.owner)

    if prop.access in gen_getter:
        context.methods += access_getter_impl.format(
            property=prop.name_cut, property_type=prop.type, type=prop.owner)

    if prop.access in gen_setter:
        context.methods += access_setter_impl.format(
            property=prop.name_cut, property_type=prop.type, type=prop.owner)

    if prop.access == "all" or prop.access == "read_only":
        context.lua_binding += lua_binding_template_get_function.format(
            module_name=context.module_name,
            property=prop.name_cut, property_type=prop.type, type=prop.owner)

    if prop.access == "all":
        context.lua_binding += lua_binding_template_set_function.format(
            module_name=context.module_name,
            property=prop.name_cut, property_type=prop.type, type=prop.owner)

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
        context.content += 'p->has_default                    = {0};\n'.format(
            prop.has_default)

    if prop.hint != "":
        context.content += "    "
        context.content += 'p->hints                          = {{{0}}};\n'.format(
            prop.hint)

    if prop.serializable == "true":
        context.content += "    "
        context.content += 'p->serializable                   = true;\n'

    if prop.property_ser_handler != "":
        context.content += "    "
        context.content += 'p->serialization_handler          = ::agea::reflection::{0};\n'.format(
            prop.property_ser_handler)

    if prop.property_des_handler != "":
        context.content += "    "
        context.content += 'p->deserialization_handler        = ::agea::reflection::{0};\n'.format(
            prop.property_des_handler)

    if prop.property_prototype_handler != "":
        context.content += "    "
        context.content += 'p->protorype_handler              = ::agea::reflection::{0};\n'.format(
            prop.property_prototype_handler)

    if prop.property_compare_handler != "":
        context.content += "    "
        context.content += 'p->compare_handler                = ::agea::reflection::{0};\n'.format(
            prop.property_compare_handler)

    if prop.property_copy_handler != "":
        context.content += "    "
        context.content += 'p->copy_handler                   = ::agea::reflection::{0};\n'.format(
            prop.property_copy_handler)

    context.content += property_template_end


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def write_lua_class_type(context: file_context, current_class: agea_class):
    if current_class.parent == '':
        context.lua_binding += lua_binding_class_template.format(
            module_name=context.module_name, type=current_class.name, lua_end='')
    else:
        context.lua_binding += lua_binding_class_template.format(
            module_name=context.module_name, type=current_class.name, lua_end=',sol::base_classes, sol::bases<' + current_class.parent + '>()')


def write_lua_struct_type(context: file_context, current_struct: agea_struct):

    ctro_line = ''
    for c in current_struct.ctros:
        ctro_line += c.name
        ctro_line += ','

    context.lua_binding += lua_binding_struct_template.format(
        module_name=context.module_name,
        type=current_struct.name, ctor_line=ctro_line[:-1]
    )


def to_type_id(type, is_parent,  context: file_context):
    if is_parent:
        return type.split("::")[-2] + "__" + type.split("::")[-1]

    return context.module_name + "__" + type


def parse_file(original_file_full_path, original_file_rel_path, module_name, context: file_context):

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

        if line.startswith("AGEA_ar_class"):
            current_class = agea_class()

            i, tokens = parse_attributes(
                "AGEA_ar_class", lines, i, lines_count)

            for t in tokens:
                pairs = t.strip().split("=")
                key = pairs[0][1:]
                value = pairs[1][:-1]

                if key == "architype":
                    current_class.architype = value

            final_tokens = []
            class_tokens = lines[i].replace(
                " : ", " ").replace("\n", " ").replace(",", " ").split(" ")
            for t in class_tokens:
                if t not in {"class", "public", "private", str()}:
                    final_tokens.append(t)

            current_class.name = final_tokens[0]
            if len(final_tokens) > 1:
                current_class.parent = final_tokens[1]

        if line.startswith("AGEA_ar_struct()"):

            if current_struct is None:
                current_struct = agea_struct()

            i = i + 1
            final_tokens = []
            class_tokens = lines[i].replace(
                " : ", " ").replace("\n", " ").replace(",", " ").split(" ")
            for t in class_tokens:
                if t not in {"struct", "public", "private", str()}:
                    final_tokens.append(t)

            current_struct.name = final_tokens[0]

        if line.startswith("AGEA_ar_function"):

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

        if line.startswith("AGEA_ar_property"):
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
                    if (len(field_tokens) < 3 or field_tokens[2] != "="):
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
            prop.name_cut = field_tokens[1][2:]
            prop.owner = class_name

            current_class.properties.append(prop)

        if line.startswith("AGEA_ar_ctor"):
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

        if line.startswith("AGEA_ar_type("):
            pos = line.find(")")
            custom_type = line[len("AGEA_ar_type("): pos]

            context.custom_types.append(custom_type)

        i = i + 1

    if current_class:
        context.classes.append(current_class)

    if current_struct:
        context.structs.append(current_struct)

    include = '#include "' + original_file_rel_path + '"'

    context.includes.add(include)


def from_custom_type(custom_type: str):
    return custom_type.split("::")[-1]


def write_types_ids_include(output_file, context):
    output = open(output_file, "w")

    total_types_size = len(context.custom_types)+len(context.types)

    if total_types_size == 0:
        return

    types_template_begin = """#pragma once

#include "{module_name}/{module_name}_types_meta_ids.ar.h"

namespace {full_module_name} {{
   enum {{
"""

    output.write(types_template_begin.format(
        module_name=context.module_name, full_module_name=context.full_module_name))

    first_written = False
    if len(context.custom_types) != 0:
        output.write('            // custom-types\n')
        output.write("            {module_name}__{type} = {module_name}__first,\n".format(
            module_name=context.module_name, type=from_custom_type(context.custom_types[0])))
        first_written = True

    for i in range(1, len(context.custom_types)):
        output.write("            {module_name}__{type},\n".format(
            module_name=context.module_name,  type=from_custom_type(context.custom_types[i])))

    start_from = 0
    if len(context.types) != 0:
        output.write('            // gen-types\n')
        if first_written == False:
            output.write("            {module_name}__{type} = {module_name}__first,\n".format(
                module_name=context.module_name, type=context.types[0]))
            start_from = 1

    for i in range(start_from, len(context.types)):
        output.write("            {module_name}__{type},\n".format(
            module_name=context.module_name,  type=context.types[i]))

    types_template_end = """
   };
}
"""
    output.write(types_template_end)


def write_types_ids_meta_include(output_file, context, offset_r,  count_r):

    output = open(output_file, "w")

    total_types_size = len(context.custom_types)+len(context.types)

    if total_types_size == 0:
        return

    types_ids_meta_template = """#pragma once

namespace {full_module_name} {{
   enum {{
            {module_name}__first = {from_r},
            {module_name}__last  = {to_r},
            {module_name}__count = {count_r}
        }};
}}
"""
    output.write(types_ids_meta_template.format(module_name=context.module_name, full_module_name=context.full_module_name,
                                                from_r=offset_r, to_r=offset_r + count_r, count_r=count_r))


def write_types_resolvers(output_file, context):
    output = open(output_file, "w")

    types_template_begin = """#pragma once


#include "core/reflection/types.h"

#include "{module_name}/{module_name}_types_ids.ar.h"

"""
    output.write(types_template_begin.format(module_name=context.module_name))

    l = list(context.includes)
    l.sort()

    for i in l:
        output.write(i)
        output.write("\n")

    ns = """
namespace agea::reflection
{
"""
    output.write(ns)

    for t in context.types:
        output.write(type_resolver.format(module_name=context.module_name,
                     full_module_name=context.full_module_name, type=t, full_type=context.full_module_name + "::" + t))

    for t in context.custom_types:
        output.write(type_resolver.format(module_name=context.module_name,
                     full_module_name=context.full_module_name, full_type=t, type=from_custom_type(t)))

    output.write("}\n")


def write_file(output_file, context: file_context, dependencies):
    output = open(output_file, "w")

    custom_include = ""
    if context.has_custom_types:
        custom_include += """#include "{module_name}/{module_name}_types_custom.h"
""".format(
            module_name=context.module_name)

    if context.has_custom_properties:
        custom_include += """#include "{module_name}/{module_name}_properties_custom.h"
""".format(
            module_name=context.module_name)

    output.write(default_header.format(
        module_name=context.module_name, custom_include=custom_include))
    output.write("\n\n")

    for d in dependencies:
        f = """#include "{module_name}/{module_name}_module.h"
#include "{module_name}/{module_name}_types_ids.ar.h"
#include "{module_name}/{module_name}_types_resolvers.ar.h"
"""
        output.write(f.format(module_name=d))

    output.write("\n\n")
    output.write("namespace {full_module_name} {{".format(
        full_module_name=context.full_module_name))

    output.write("\n\n")

    for t in context.types:
        output.write("static ::agea::reflection::reflection_type {module_name}_{type}_rt;\n".format(
            module_name=context.module_name, type=t))

    for t in context.custom_types:
        output.write("static ::agea::reflection::reflection_type {module_name}_{type}_rt;\n".format(
            module_name=context.module_name, type=from_custom_type(t)))

    output.write("\n\n")
    output.write(context.soal)
    output.write("\n\n")

    output.write(default_content.format(module_name=context.module_name))
    output.write("\n\n")

    for t in context.custom_types:
        output.write(empty_template_no_class.format(
            module_name=context.module_name, type=t, short_type=from_custom_type(t)))
        output.write("\n}\n")

    output.write(context.empty_cache)
    output.write("\n\n")
    output.write(context.parent)
    output.write("\n\n")
    for t in context.types:
        output.write("    {module_name}_{type}_rt.update();\n".format(
            module_name=context.module_name, type=t))
    output.write("\n\n")
    output.write(context.reg_types)
    output.write("\n\n")
    output.write(context.content)
    output.write("\n\n")
    output.write(context.lua_binding)
    output.write("\n\n")
    output.write(context.footer)
    output.write("\n\n")
    output.write(context.methods)
    output.write("\n\n")
    output.write(modules_instance_template.format(
        module_name=context.module_name))
    output.write("\n\n")
    output.write("}")


access_getter = """public:\\
{property_type} get_{property}() const;\\
private:\\
"""
access_setter = """public:\\
void set_{property}({property_type} v);\\
private:\\
"""


def write_type_include_file(ar_class: agea_class, context: file_context, output_dir):

    full_file_path = os.path.basename(ar_class.name) + ".generated.h"

    header_file_content = f"""
#pragma once

#include "{context.module_name}/{context.module_name}_module.h"

#define AGEA_gen_meta__{ar_class.name}()   \\
    friend class {context.module_name}_module; \\
"""

    for prop in ar_class.properties:

        if prop.access in gen_getter:
            header_file_content += access_getter.format(
                property=prop.name_cut, property_type=prop.type)

        if prop.access in gen_setter:
            header_file_content += access_setter.format(
                property=prop.name_cut, property_type=prop.type)

    full_file_path = os.path.join(
        output_dir, context.module_name, full_file_path)
    with open(full_file_path, 'w') as file:
        file.write(header_file_content)


def main(ar_cfg_path, root_dir, output_dir, module_name, module_namespace):

    print("SOLing : SOL config - {0}, root dir - {1}, output - {2}, package_name - {3}, module_namespace - {4}".format(
        ar_cfg_path, root_dir, output_dir, module_name, module_namespace))

    module_namespace = module_namespace.strip()

    context = file_context(module_name, module_namespace)

    context.has_custom_types = os.path.exists(
        os.path.join(
            root_dir, "include", module_name, module_name + "_types_custom.h"))

    context.has_custom_properties = os.path.exists(
        os.path.join(root_dir,  "include", module_name, module_name + "_properties_custom.h"))

    cfg = open(ar_cfg_path, 'r', newline='\n')
    lines = cfg.readlines()
    for f in lines:
        f = extstrip(f)
        if len(f) > 0:
            file_path = os.path.join(root_dir, f).replace("\\", "/")
            parse_file(file_path, f, module_name, context)

    for c in context.classes:
        write_type_include_file(c, context, output_dir)

        context.soal += soal_template.format(
            module_name=module_name, type=c.name)

        context.empty_cache += empty_template.format(
            module_name=module_name, type=c.name, short_type=c.name, child=c.name)

        if c.architype:
            context.empty_cache += "    rt.arch         = core::architype::{architype};\n".format(
                architype=c.architype)

        context.reg_types += smart_object_reg_package_type.format(
            type=c.name, child=c.name)

        context.empty_cache += "}\n"

        if len(c.parent) > 0:
            context.parent += empty_template_with_parent.format(
                module_name=module_name, type=c.name, child=c.name, parent=c.parent)

        write_lua_class_type(context, c)

        for p in c.properties:
            write_properties(context, p, c)

        for p in c.functions:
            context.lua_binding += """       lua_type["{0}"] = &{1}::{0};
    """.format(p.name, c.name)

        context.lua_binding += lua_binding_template_end
        context.types.append(c.name)

    for s in context.structs:

        write_lua_struct_type(context, s)

        for p in s.functions:
            context.lua_binding += """       lua_type["{0}"] = &{1}::{0};
    """.format(p.name, s.name)

        context.lua_binding += lua_binding_template_end

        output_file = os.path.join(
            output_dir, module_name,  module_name + ".ar.cpp")

    context.types.sort()
    context.custom_types.sort()

    arl = agea_range_list(root_dir, output_dir)

    arl.load()

    types_count = len(context.custom_types) + len(context.types)

    ri = arl.update(module_name, types_count)

    if ri != -1:

        write_file(output_file, context, arl.ranges[ri].dependency)

        output_file = os.path.join(
            output_dir, module_name,  module_name + "_types_ids.ar.h")

        for i in range(ri, len(arl.ranges)):
            meta_ids_file = os.path.join(
                output_dir, module_name,  module_name + "_types_meta_ids.ar.h")

            r = arl.ranges[i]

            write_types_ids_meta_include(
                meta_ids_file,  context, r.offset, r.count)

    write_types_ids_include(output_file, context)

    output_file = os.path.join(
        output_dir, module_name,  module_name + "_types_resolvers.ar.h")

    write_types_resolvers(output_file, context)

    arl.save()


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3],
         sys.argv[4], sys.argv[5])
