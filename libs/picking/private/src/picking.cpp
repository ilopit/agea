#include "picking/picking.h"

#include "packages/root/model/components/camera_component.h"

#include <core/level.h>
#include <core/model_system.h>
#include <global_state/global_state.h>
#include <native/native_window.h>

namespace kryga
{
namespace picking
{

root::camera_component*
find_active_camera()
{
    auto* lvl = glob::glob_state().getr_model().current_level;
    if (!lvl)
    {
        return nullptr;
    }
    // The level remembers its active camera (written on load + activation), so this is
    // an O(1) id resolve — no scene scan. Null if unset or the camera was destroyed
    // (find_component yields null for a stale id).
    auto* c = lvl->find_component(lvl->get_active_camera_id());
    return c ? c->as<root::camera_component>() : nullptr;
}

root::game_object*
pick_object_under_cursor(root::camera_component& cam, int32_t mouse_x, int32_t mouse_y)
{
    auto* lvl = glob::glob_state().getr_model().current_level;
    if (!lvl)
    {
        return nullptr;
    }

    const auto sz = glob::glob_state().get_native_window()->get_size();
    return lvl->pick_under_cursor(cam.get_inv_projection(),
                                  cam.get_view(),
                                  mouse_x,
                                  mouse_y,
                                  static_cast<uint32_t>(sz.w),
                                  static_cast<uint32_t>(sz.h));
}

}  // namespace picking
}  // namespace kryga
