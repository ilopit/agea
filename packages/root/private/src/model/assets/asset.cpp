#include "packages/root/model/assets/mesh.h"

#include <core/model_system.h>
#include <global_state/global_state.h>

namespace kryga
{
namespace root
{

KRG_gen_class_cd_default(asset);

void
asset::mark_render_dirty()
{
    if (get_state() != smart_object_state::constructed)
    {
        glob::glob_state().getr_model().output.dirty_render.emplace_back(this);
        set_state(smart_object_state::constructed);
    }
}

}  // namespace root
}  // namespace kryga
