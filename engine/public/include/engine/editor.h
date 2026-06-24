#pragma once

#include <vulkan_render/types/vulkan_gpu_types.h>

#include <utils/check.h>
#include <utils/id.h>

namespace kryga
{
namespace core
{
struct io_context;
}

namespace root
{
class game_object;
}

namespace root
{
class camera_component;
class destructible_mesh_component;
}  // namespace root

namespace engine
{

enum class editor_mode
{
    editor,
    playing
};

class game_editor
{
public:
    void
    init();

    void
    set_selected(const utils::id& id);

    utils::id
    get_selected() const;

    root::game_object*
    get_selected_game_object() const;

    void
    on_tick(float dur_sec);

    gpu::camera_data
    get_camera_data();

    void
    set_camera(glm::vec3 position, float pitch, float yaw);

    void
    update_camera();

    void
    ev_move_forward(float f);

    void
    ev_move_left(float f);

    void
    ev_look_up(float f);

    void
    ev_look_left(float f);

    void
    ev_mouse_press(const core::io_context& e);

    void
    ev_reload();

    void
    ev_spawn();

    void
    ev_lights();

    void
    ev_shatter_demo();

    editor_mode
    get_mode() const;

    void
    enter_play_mode();

    void
    exit_play_mode();

    void
    ev_toggle_play();

    void
    ev_escape();

    root::camera_component*
    get_active_camera() const;

private:
    glm::mat4
    get_rotation_matrix();

    void
    apply_pick(const utils::id& clicked_id);

    glm::vec3 m_position;
    glm::vec3 m_velocity;

    float m_pitch = -12.f;
    float m_yaw = 0.f;

    float m_forward_delta = 0.f;
    float m_left_delta = 0.f;
    float m_look_up_delta = 0.f;
    float m_look_left_delta = 0.f;

public:
    int m_camera_speed_power = 3;

    float
    get_camera_speed_multiplier() const
    {
        constexpr float lut[] = {0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
        return lut[m_camera_speed_power];
    }

private:
    float m_cached_aspect_ratio = 0.f;
    gpu::camera_data m_camera_data;

    bool m_updated = true;

    editor_mode m_mode = editor_mode::editor;
    glm::vec3 m_saved_position;
    float m_saved_pitch = 0.f;
    float m_saved_yaw = 0.f;
    bool m_saved_grid_visible = true;
    bool m_saved_editor_mode_visuals = true;

    root::camera_component* m_active_camera = nullptr;
    root::destructible_mesh_component* m_pending_shatter = nullptr;
    int m_pending_shatter_frames = 0;

    // Selection state. RPC handlers route through the engine's main-thread
    // action queue, so this is single-thread-owned — no mutex needed.
    utils::id m_selected;
};
}  // namespace engine
}  // namespace kryga
