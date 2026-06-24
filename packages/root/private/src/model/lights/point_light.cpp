#include "packages/root/model/lights/point_light.h"

#include <packages/root/model/components/mesh_component.h>
#include <packages/root/model/lights/components/point_light_component.h>

namespace kryga
{
namespace root
{

KRG_gen_class_cd_default(point_light);

bool
point_light::construct(construct_params& params)
{
    if (!base_class::construct(params))
    {
        return false;
    }

    // Billboard debug component spawned from level AOBJ, not here
    // (spawning here causes duplicate mesh registration)

    spawn_component<point_light_component>(m_root_component, AID("lc"), {});

    return true;
}

}  // namespace root
}  // namespace kryga
