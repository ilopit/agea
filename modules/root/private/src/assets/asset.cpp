#include "root/assets/mesh.h"

#include "core/level.h"

namespace agea
{
namespace root
{

AGEA_gen_class_cd_default(asset);

void
asset::mark_render_dirty()
{
    if (get_state() != smart_object_state::constructed)
    {
        glob::level::getr().add_to_dirty_render_assets_queue(this);
        set_state(smart_object_state::constructed);
    }
}

}  // namespace root
}  // namespace agea
