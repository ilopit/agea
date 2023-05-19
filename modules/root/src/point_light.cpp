#include "root/point_light.h"

#include <root/components/mesh_component.h>
#include <root/components/light_component.h>

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

    spawn_component_with_proto(m_root_component, AID("bulb_component"), AID("AB"));

    spawn_component<light_component>(m_root_component, AID("lc"), {});

    return true;
}

void
point_light::on_tick(float dt)
{
    static float f = dt;
    static bool dir = true;

    f += dt;

    if (f > 30.f)
    {
        dir = !dir;
        f = 0.f;
    }
    vec3 g{0.f, dt * (dir ? -1.f : 1.f), 0.f};
    // move(g);
}

}  // namespace root
}  // namespace agea
