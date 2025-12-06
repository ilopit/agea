#include "packages/root/model/assets/shader_effect.h"

#include "core/level.h"
#include <global_state/global_state.h>

namespace agea
{
namespace root
{

AGEA_gen_class_cd_default(shader_effect);

void
shader_effect::mark_render_dirty()
{
    if (get_state() != smart_object_state::constructed)
    {
        glob::glob_state().get_current_level()->add_to_dirty_shader_effect_queue(this);
        set_state(smart_object_state::constructed);
    }
}

}  // namespace root
}  // namespace agea
