#include "packages/root/model/ui_widget.h"

#include <core/model_system.h>
#include <global_state/global_state.h>

namespace kryga
{
namespace root
{

KRG_gen_class_cd_default(ui_widget);

bool
ui_widget::construct(construct_params& c)
{
    KRG_return_false(base_class::construct(c));
    return true;
}

void
ui_widget::mark_render_dirty()
{
    if (get_state() != smart_object_state::constructed)
    {
        glob::glob_state().getr_model().queue_render_dirty(this);
        set_state(smart_object_state::constructed);
    }
}

}  // namespace root
}  // namespace kryga
