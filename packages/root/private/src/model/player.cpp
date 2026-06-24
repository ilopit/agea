#include "packages/root/model/player.h"

#include "packages/root/model/components/camera_component.h"
#include "packages/root/model/components/input_component.h"

#include <core/model_system.h>
#include <global_state/global_state.h>

namespace kryga
{
namespace root
{

KRG_gen_class_cd_default(player);

bool
player::construct(this_class::construct_params& p)
{
    if (!base_class::construct(p))
    {
        return false;
    }

    camera_component::construct_params ccp;
    m_camera = spawn_component<camera_component>(get_root_component(), AID("camera"), ccp);

    input_component::construct_params icp;
    m_input = spawn_component<input_component>(get_root_component(), AID("input"), icp);

    return true;
}

}  // namespace root
}  // namespace kryga
