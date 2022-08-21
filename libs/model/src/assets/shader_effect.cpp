#include "model/assets/shader_effect.h"

#include "model/level.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(shader_effect);

void
shader_effect::mark_render_dirty()
{
    if (get_state() != smart_objet_state__constructed)
    {
        glob::level::getr().add_to_dirty_shader_effect_queue(this);
        set_state(smart_objet_state__constructed);
    }
}

}  // namespace model
}  // namespace agea
