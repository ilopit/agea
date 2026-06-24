#pragma once

#include <cstdint>

namespace kryga
{
namespace root
{
class game_object;
}

namespace root
{
class camera_component;
}

namespace picking
{

// The current level's active camera, or nullptr if none. O(1) resolve of the id the
// level remembers (set on camera load + activation); no scene scan. Pairs with
// pick_object_under_cursor — typically: cam = find_active_camera(); pick(*cam, ...).
root::camera_component*
find_active_camera();

// Camera-aware convenience over core::level::pick_under_cursor: pulls the view /
// projection matrices off the camera and the viewport size off the active window, then
// delegates the ray build + spatial-index query to the current level. Returns the
// nearest hit game_object, or nullptr on a miss.
//
// Lives in its own lib (not in base/core) because it couples base's camera_component
// with the level's pick index, so every game shares one path instead of each
// re-deriving the ray. root::find_active_camera() gives the cam.
root::game_object*
pick_object_under_cursor(root::camera_component& cam, int32_t mouse_x, int32_t mouse_y);

}  // namespace picking
}  // namespace kryga
