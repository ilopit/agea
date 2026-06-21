#include "packages/nevermatch/model/door_trigger.h"

#include <utils/kryga_log.h>

namespace kryga::nevermatch
{

KRG_gen_class_cd_default(door_trigger);

bool
door_trigger::construct(construct_params&)
{
    return true;
}

void
door_trigger::begin_play()
{
    // Super:: contract — keep component propagation.
    game_object::begin_play();
    ALOG_INFO("door_trigger '{}' begin_play, target level '{}'",
              get_id().str(),
              m_target_level.str());
}

void
door_trigger::on_tick(float)
{
}

}  // namespace kryga::nevermatch
