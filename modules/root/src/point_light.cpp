#include "root/point_light.h"

#include <root/components/mesh_component.h>

#include <root/assets/mesh.h>
#include <root/assets/material.h>

#include <core/caches/cache_set.h>
#include <core/caches/materials_cache.h>
#include <core/caches/meshes_cache.h>

namespace agea
{
namespace root
{

AGEA_gen_class_cd_default(point_light);

bool
point_light::construct(construct_params& params)
{
    if (!base_class::construct(params))
    {
        return false;
    }

    auto com = spawn_component_with_proto(m_root_component, AID("bulb_component"), AID("AB"));

    return true;
}

void
point_light::on_tick(float dt)
{
    static float f = dt;

    f += dt;

    vec3 g{0.f, f * 1, 0.f};

    // set_rotation(g);
}

}  // namespace root
}  // namespace agea
