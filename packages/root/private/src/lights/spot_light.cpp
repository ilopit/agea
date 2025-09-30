#include "packages/root/lights/spot_light.h"

#include <packages/root/components/mesh_component.h>
#include <packages/root/lights/components/spot_light_component.h>

#include <packages/root/assets/mesh.h>
#include <packages/root/assets/material.h>

#include <core/caches/cache_set.h>
#include <core/caches/materials_cache.h>
#include <core/caches/meshes_cache.h>

namespace agea
{
namespace root
{

AGEA_gen_class_cd_default(spot_light);

bool
spot_light::construct(construct_params& params)
{
    if (!base_class::construct(params))
    {
        return false;
    }

    spawn_component_with_proto(m_root_component, AID("spot_light_debug_component"));

    spawn_component<spot_light_component>(m_root_component, AID("spot_light_component"), {});

    return true;
}

void
spot_light::on_tick(float dt)
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
