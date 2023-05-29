import os
import shutil
import sys

example_header_template = """

#pragma once

#include "{0}/example.generated.h"

#include "core/model_minimal.h"
#include "root/core_types/vec3.h"
#include "root/game_object.h"

namespace agea
{{
namespace root
{{
class mesh_component;
class point_light_component;
}}  // namespace root
}}  // namespace agea

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

    bool
    construct(construct_params& params);

    void
    on_tick(float dt);

    float m_time = 0.f;

    agea::root::mesh_component* m_c1 = nullptr;
    agea::root::mesh_component* m_c2 = nullptr;
    agea::root::mesh_component* m_c3 = nullptr;
    agea::root::mesh_component* m_c4 = nullptr;
    agea::root::mesh_component* m_c5 = nullptr;

    agea::root::point_light_component* m_plc = nullptr;
}};

}}  // namespace {0}

"""

example_source_template = """#include "{0}/example.h"

#include <root/components/mesh_component.h>
#include <root/lights/components/point_light_component.h>
#include <root/assets/solid_color_material.h>

#include <core/caches/cache_set.h>
#include <core/caches/meshes_cache.h>
#include <core/caches/materials_cache.h>

using namespace agea;

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
    AGEA_return_false(base_class::construct(params));

    root::mesh_component::construct_params mc;

    mc.mesh_handle = glob::meshes_cache::getr().get_item(AID("cube_mesh"));
    mc.material_handle = glob::materials_cache::getr().get_item(AID("mt_red"));

    AGEA_check(mc.mesh_handle, "We should have a mesh!");
    AGEA_check(mc.material_handle, "We should have a material!");

    m_c1 = spawn_component<root::mesh_component>(m_root_component, AID("ec1"), mc);

    m_c2 = spawn_component<root::mesh_component>(m_c1, AID("ec2"), mc);
    m_c2->set_position(root::vec3{{5.f, 0.f, 5.f}});

    m_c3 = spawn_component<root::mesh_component>(m_c2, AID("ec3"), mc);
    m_c3->set_position(root::vec3{{5.f, 0.f, 5.f}});

    m_c4 = spawn_component<root::mesh_component>(m_root_component, AID("ec4"), mc);
    m_c4->set_position(root::vec3{{10.f, 0.f, 5.f}});

    mc.material_handle = glob::materials_cache::getr().get_item(AID("mt_solid_color"));
    m_c5 = spawn_component<root::mesh_component>(m_root_component, AID("ec5"), mc);
    m_c5->set_position(root::vec3{{10.f, 0.f, 10.f}});

    m_plc = spawn_component<root::point_light_component>(m_root_component, AID("ec5"), {{}});
    m_plc->set_position(root::vec3{{0.f, 20.f, 0.f}});

    return true;
}}

void
example::on_tick(float dt)
{{
    m_time += dt;

    {{
        root::vec3 v{{0.5f * dt, 0.f, 0.f}};
        m_c1->rotate(v);
    }}
    {{
        auto t = glm::mod(m_time, 20.f);

        m_c2->set_material(t > 10.0 ? glob::materials_cache::getr().get_item(AID("mt_solid_color"))
                                  : glob::materials_cache::getr().get_item(AID("mt_red")));
    }}
    {{
        auto t = glm::mod(m_time, 20.f);

        m_c3->set_material(t > 5.0 ? glob::materials_cache::getr().get_item(AID("mt_wire"))
                                 : glob::materials_cache::getr().get_item(AID("mt_red")));
    }}
    {{
        auto t = glm::mod(m_time, 20.f);
        auto dir = t > 10.0 ? -1.f : 1.f;
        root::vec3 v{{0.f, dir * dt, 0.f}};
        m_c4->move(v);
    }}

    if (auto scm = m_c5->get_material()->as<root::solid_color_material>())
    {{
        auto t0 = glm::mod(m_time, 128.f);
        auto t1 = glm::mod(m_time, 50.f) / 50.f;
        auto t2 = glm::mod(m_time, 100.f) / 100.f;
        auto t3 = glm::mod(m_time, 200.f) / 200.f;
        auto t4 = glm::mod(m_time, 400.f) / 400.f;

        scm->set_shininess(t0);
        scm->set_ambient(glm::vec3{{t2, t3, t4}});
        scm->set_diffuse(glm::vec3{{t2, t4, t1}});
        scm->set_specular(glm::vec3{{t1, t3, t2}});
        scm->mark_render_dirty();
    }}
}}

}}  // namespace {0}

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

#define AGEA_{module_name}_module_included

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


    agea_root_dir = os.path.dirname(
        os.path.dirname(os.path.realpath(__file__)))


    modules_path = os.path.join(agea_root_dir, "modules")
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
