#include "packages/base/model/lights/directional_light.h"

#include <packages/base/model/components/mesh_component.h>
#include <packages/base/model/lights/components/directional_light_component.h>

#include <packages/root/model/assets/mesh.h>
#include <packages/root/model/assets/material.h>

#include <core/caches/cache_set.h>

namespace kryga
{
namespace base
{

KRG_gen_class_cd_default(directional_light);

bool
directional_light::construct(construct_params& params)
{
    if (!base_class::construct(params))
    {
        return false;
    }

    // Billboard debug component spawned from level AOBJ, not here

    auto dl = spawn_component_with_proto(m_root_component, AID("directional_light_component"));

    return true;
}

void
directional_light::on_tick(float dt)
{
    static float f = dt;
    static bool dir = true;

    f += dt;

    if (f > 30.f)
    {
        dir = !dir;
        f = 0.f;
    }
    ::kryga::root::vec3 g{0.f, dt * (dir ? -1.f : 1.f), 0.f};
    // move(g);
}

}  // namespace base
}  // namespace kryga
