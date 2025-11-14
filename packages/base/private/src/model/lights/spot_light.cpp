#include "packages/base/model/lights/spot_light.h"

#include <packages/base/model/components/mesh_component.h>
#include <packages/base/model/lights/components/spot_light_component.h>

#include <packages/root/model/assets/mesh.h>
#include <packages/root/model/assets/material.h>

#include <core/caches/cache_set.h>

namespace agea
{
namespace base
{

AGEA_gen_class_cd_default(spot_light);

bool
spot_light::construct(construct_params& params)
{
    if (!base_class::construct(params))
    {
        return false;
    }

    /*


    spawn_component_with_proto(m_root_component, AID("spot_light_debug_component"));

    spawn_component<spot_light_component>(m_root_component, AID("spot_light_component"), {});

    */

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
    // vec3 g{0.f, dt * (dir ? -1.f : 1.f), 0.f};
    //  move(g);
}

}  // namespace base
}  // namespace agea
