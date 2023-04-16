#include "demo/example.h"

#include <root/components/mesh_component.h>

#include <root/assets/mesh.h>
#include <root/assets/material.h>

#include <core/caches/caches_map.h>
#include <core/caches/materials_cache.h>
#include <core/caches/meshes_cache.h>

namespace demo
{

example::example()
{
}

example::~example()
{
}

bool
example::construct(construct_params& params)
{
    base_class::construct(params);

    agea::root::mesh_component::construct_params mpc{};

    auto com =
        spawn_component<agea::root::mesh_component>(nullptr, AID("mesh_component"), AID("AB"), mpc);

    com->set_mesh(::agea::glob::meshes_cache::getr().get_item(AID("cube_mesh")));
    com->set_material(::agea::glob::materials_cache::getr().get_item(AID("mt_red")));

    m_root_component = com;

    return true;
}

}  // namespace demo
