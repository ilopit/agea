#include "packages/base/model/lights/point_light.h"

#include <packages/base/model/components/mesh_component.h>
#include <packages/base/model/lights/components/point_light_component.h>

namespace kryga
{
namespace base
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

}  // namespace base
}  // namespace kryga
