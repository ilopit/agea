#include "packages/root/model/lights/point_light.h"

#include <packages/root/model/components/mesh_component.h>
#include <packages/root/model/lights/components/point_light_component.h>

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

    spawn_component_with_proto(m_root_component, AID("bulb_component"));

    spawn_component<point_light_component>(m_root_component, AID("lc"), {});

    return true;
}

}  // namespace root
}  // namespace agea
