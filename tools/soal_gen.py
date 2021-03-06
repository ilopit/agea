import sys
import os


default_header = """// Smart Object Autogenerated Layout

// clang-format off

#include "include/model/caches/empty_objects_cache.h"

#include "model/reflection/type_handlers/property_type_copy_handlers.h"
#include "model/reflection/type_handlers/property_type_serialization_handlers.h"
#include "model/reflection/type_handlers/property_type_serialization_update_handlers.h"
#include "model/reflection/type_handlers/property_type_serialization_handlers_custom.h"
#include "model/reflection/type_handlers/property_type_compare_handlers.h"
"""

default_content = """
bool 
agea::reflection::entry::set_up()
{
    property_type_copy_handlers::init();
    property_type_serialization_handlers::init();
    property_type_serialization_update_handlers::init();
    property_type_compare_handlers::init();

    glob::empty_objects_cache::s_closure = std::move(glob::empty_objects_cache::create());
"""

default_footer = """
    object_reflection::fill_properties();
    return true;
}
// clang-format on
"""

soal_template = """
agea::utils::id
agea::model::{0}::META_type_id()
{{
    return agea::utils::id::from("{0}");
}}

void 
agea::model::{0}::META_class_set_type_id()
{{
    m_type_id = META_type_id();
}}

void 
agea::model::{0}::META_class_set_architype_id()
{{
    m_architype_id = META_architype_id();
}}

::agea::reflection::object_reflection*
agea::model::{0}::META_object_reflection()
{{
    static ::agea::reflection::object_reflection rt{{
        std::is_same<this_class, base_class>::value
            ? nullptr
            : base_class::META_object_reflection(), META_type_id()}};
    return &rt;
}}

::agea::reflection::object_reflection*
agea::model::{0}::reflection() const
{{
    return this_class::META_object_reflection();
}}

bool
agea::model::{0}::META_construct(smart_object::construct_params& i)
{{
    /* Replace to dynamic cast */
    this_class::construct_params* cp = (this_class::construct_params*)&i;

    return construct(*cp);
}}

bool
agea::model::{0}::META_post_construct()
{{
    return post_construct();
}}

std::shared_ptr<::agea::model::smart_object>
agea::model::{0}::META_create_empty_obj()
{{
    return this_class::META_class_create_empty_obj();
}}

std::shared_ptr<::agea::model::{0}>
agea::model::{0}::META_class_create_empty_obj()
{{
    auto s = std::make_shared<this_class>();
    s->META_class_set_type_id();
    s->META_class_set_architype_id();
    return s;
}}

"""

empty_template = """
{{
    using type = ::agea::model::{0};
    ::agea::glob::empty_objects_cache::get()->insert(type::META_type_id(), type::META_class_create_empty_obj());
    auto dummy = type::META_object_reflection();
    (void)dummy;
}}
"""


property_template_start = """
{{
    using type       = ::agea::model::{2};

    auto prop        = std::make_shared<property>();
    auto p           = prop.get();
    auto table       = type::META_object_reflection();

    table->m_properties.emplace_back(std::move(prop));

    // Main fields
    p->name                           = "{0}";
    p->offset                         = offsetof(type, m_{0});
    p->type                           = ::agea::reflection::type_resolver::resolve<decltype(type::m_{0})>();
    p->types_compare_handler          = property_type_compare_handlers::compare_handlers()[(size_t)p->type.type];\n
    // Extra fields
"""
property_template_end = """
}
"""

def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


class file_context:
    def __init__(self):
        self.header = default_header
        self.includes = set()
        self.content = ""
        self.footer = default_footer
        self.soal = ""
        self.empty_cache = ""


class property:
    def __init__(self):
        self.name = ""
        self.category = ""
        self.type = ""
        self.access = ""
        self.owner = ""
        self.hint = ""
        self.visible = ""
        self.serializable = "no"
        self.property_ser_handler = ""
        self.property_des_handler = ""
        self.property_prototype_handler = ""
        self.property_compare_handler = ""
        self.property_copy_handler = ""
        self.copyable = "yes"
        self.updatable = "yes"

def process_file(original_file_full_path, original_file_rel_path, context):

    eprint("processing : {0} ...".format(original_file_full_path))

    class_name = os.path.basename(original_file_full_path)[:-2]

    cfg = open(original_file_full_path, 'r')
    lines = cfg.readlines()
    lines_count = len(lines)


    context.soal += soal_template.format(class_name)

    context.empty_cache += empty_template.format(class_name)

    i = 0
    while i != lines_count:
        line = lines[i].strip()

        property_like = ""

        #TODO, rewrite!
        if line.startswith("AGEA_property"):
            property_like += line + " "
            
            while i <= lines_count and lines[i].find(")") == -1:
                i = i + 1
                property_like += lines[i].strip() + " "


            i = i + 1
            field = lines[i].strip()[:-1].split()

            prop = property()
            property_raw = property_like[property_like.find("(") + 1:property_like.find(")")].split(", ")

            for pf in property_raw:
                pairs = pf.split("=")
                eprint("DBG! {0}".format(pairs))
                pairs[0] = pairs[0][1:]
                pairs[1] = pairs[1][:-1]

                if len(pairs) != 2:
                    eprint("Wrong numbers of pairs! {0}".format(pairs))
                    exit(-1)

                if pairs[0] == "category":
                    prop.category = pairs[1]
                elif pairs[0] == "visible":
                    prop.visible = pairs[1]
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
                elif pairs[0] == "copyable":
                    prop.copyable = pairs[1]
                elif pairs[0] == "updatable":
                    prop.copyable = pairs[1]
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
                    eprint("Unsupported property = " +  pairs[0])
                    exit(-1)

            prop.type = field[0]
            prop.name = field[1]
            prop.owner = class_name

            context.content += property_template_start.format(prop.name[2:], prop.type, prop.owner)


            if prop.access != "":
                context.content += "    "
                context.content += 'p->access                         = ::agea::reflection::access_mode::{0};\n'.format(prop.access)

            if prop.category != "":
                context.content += "    "
                context.content += 'p->category                       = "{0}";\n'.format(prop.category)

            if prop.hint != "":
                context.content += "    "
                context.content += 'p->hints                          = {{{0}}};\n'.format(prop.hint)

            if prop.visible != "":
                context.content += "    "
                context.content += 'p->visible                        = {0};\n'.format(prop.visible)

            if prop.serializable == "true":
                context.content += "    "
                context.content += 'p->types_serialization_handler    = property_type_serialization_handlers::serializers()[(size_t)p->type.type];\n'
                context.content += "    "
                context.content += 'p->types_deserialization_handler  = property_type_serialization_handlers::deserializers()[(size_t)p->type.type];\n'

            if prop.property_ser_handler != "":
                context.content += "    "
                context.content += 'p->serialization_handler          = {0};\n'.format(prop.property_ser_handler)

            if prop.property_des_handler != "":
                context.content += "    "
                context.content += 'p->deserialization_handler        = {0};\n'.format(prop.property_des_handler)

            if prop.property_prototype_handler != "":
                context.content += "    "
                context.content += 'p->protorype_handler              = {0};\n'.format(prop.property_prototype_handler)

            if prop.property_compare_handler != "":
                context.content += "    "
                context.content += 'p->compare_handler                = {0};\n'.format(prop.property_compare_handler)

            if prop.property_copy_handler != "":
                context.content += "    "
                context.content += 'p->copy_handler                   = {0};\n'.format(prop.property_copy_handler)

            if prop.copyable != "no":
                context.content += "    "
                context.content += 'p->types_copy_handler             = property_type_copy_handlers::copy_handlers()[(size_t)p->type.type];\n'

            if prop.updatable != "no":
                context.content += "    "
                context.content += 'p->types_update_handler           = property_type_serialization_update_handlers::deserializers()[(size_t)p->type.type];\n'
            context.content += property_template_end
        i = i + 1

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
    output.write(context.footer)


def main(sol_cfg_path, root_dir, output_file):

    print("SOLing : SOL config - {0}, root dir - {1}, output - {2}".format(
        sol_cfg_path, root_dir, output_file))

    context = file_context()
    cfg = open(sol_cfg_path, 'r')
    lines = cfg.readlines()
    for f in lines:
        if len(f) > 0:
            process_file(os.path.join(
                root_dir, f[:-1]).replace("\\", "/"), f[:-1], context)

    write_file(output_file, context)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3])
