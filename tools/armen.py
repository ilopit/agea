import os
import shutil
import sys

example_header_template = """
#pragma once

#include "{0}/example.generated.h"

#include "core/model_minimal.h"
#include "root/core_types/vec3.h"
#include "root/game_object.h"

namespace {0}
{{
AGEA_ar_class();
class example : public ::agea::root::game_object
{{
    AGEA_gen_meta__example();

public:
    // Meta part
    AGEA_gen_class_meta(example, ::agea::root::game_object);
    AGEA_gen_meta_api;
    AGEA_gen_construct_params{{

    }};

    bool construct(construct_params& params);

}};

}}  // namespace {0}
"""

example_source_template = """#include "{0}/example.h"

namespace {0}
{{

example::example()
{{
}}

example::~example()
{{
}}

bool
example::construct(construct_params& params)
{{
    return base_class::construct(params);
}}

}}

"""

ar_config = "include/{0}/example.h"

ar_module_config = """
{
    "dependency" : ["root"],
    "has_render" : false
}
"""

cmake_file = """
##### Reflection
add_custom_target({0}.module.ar ALL)
agea_ar_target({0}.module.ar {0} " " 100)


##### Model

set(module_model "{0}.module.model")

file(GLOB {1}_SOURCES "include/{0}/*.h" "src/*.cpp")
file(GLOB GENERATED_SRC "${{CMAKE_BINARY_DIR}}/agea_generated/{0}/*.cpp")

source_group("{0}_sources" FILES  ${{{1}_SOURCES}})

add_library(${{module_model}} STATIC
   ${{{1}_SOURCES}}         
   
   ${{GENERATED_SRC}}
)

set(AGEA_ACTIVE_MODULES_TARGETS ${{AGEA_ACTIVE_MODULES_TARGETS}} ${{module_model}} CACHE INTERNAL "")
set(AGEA_ACTIVE_MODULES ${{AGEA_ACTIVE_MODULES}} {0} CACHE INTERNAL "")

target_compile_options(${{module_model}} PRIVATE /bigobj)

target_link_libraries(${{module_model}} PUBLIC
   agea::ar
   agea::utils
   agea::glm_unofficial
   agea::core
   agea::root.module.model
   agea::serialization
   agea::resource_locator

   lua_static
   agea::sol2_unofficial
)

agea_finalize_library(${{module_model}})

target_include_directories(${{module_model}} PUBLIC ${{CMAKE_BINARY_DIR}}/agea_generated)
"""

module_template = """
#pragma once

#include "core/reflection/module.h"

namespace {module_name}
{{
class {module_name}_module : public ::agea::reflection::module
{{
public:
    {module_name}_module(const ::agea::utils::id& id)
       : ::agea::reflection::module(id)
    {{
    }}

    virtual bool
    override_reflection_types()
    {{
        return true;
    }}

    virtual bool init_reflection();

    static {module_name}_module& instance();
}};

}}  // namespace {module_name}
"""


if __name__ == "__main__":
    module_name = sys.argv[1]

    print(module_name)

    modules_path = os.path.join(os.path.abspath(os.path.curdir), "modules")
    module_path = os.path.join(modules_path, module_name)

    print(module_path)

    if os.path.exists(module_path):
        shutil.rmtree(module_path)

    include_folder = os.path.join(module_path, "include", module_name)
    src_folder = os.path.join(module_path, "src")
    test_folder = os.path.join(module_path, "test")
    ar_folder = os.path.join(module_path, "ar")

    os.makedirs(include_folder)
    print(include_folder)
    os.makedirs(src_folder)
    print(src_folder)
    os.makedirs(test_folder)
    print(test_folder)
    os.makedirs(ar_folder)
    print(ar_folder)

    with open(os.path.join(include_folder, "example.h"), "w") as w:
        w.write(example_header_template.format(module_name))

    with open(os.path.join(include_folder, module_name + "_module.h"), "w") as w:
        w.write(module_template.format(module_name=module_name))

    with open(os.path.join(src_folder, "example.cpp"), "w") as w:
        w.write(example_source_template.format(module_name))

    with open(os.path.join(ar_folder, "config"), "w") as w:
        w.write(ar_config.format(module_name))

    with open(os.path.join(ar_folder, "module"), "w") as w:
        w.write(ar_module_config)

    with open(os.path.join(module_path, "CMakeLists.txt"), "w") as w:
        w.write(cmake_file.format(module_name, module_name.upper()))

    module_includes = []
    for d in os.listdir(modules_path):
        if os.path.isdir(os.path.join(modules_path, d)):
            module_includes.append(f"add_subdirectory({str(d)})\n")

    with open(os.path.join(modules_path, "CMakeLists.txt"), "w", newline='\n') as w:
        w.writelines(module_includes)
