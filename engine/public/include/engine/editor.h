#pragma once

#include <vulkan_render/types/vulkan_gpu_types.h>

#include <utils/singleton_instance.h>

namespace kryga
{
namespace base
{
class camera_component;
class input_component;
}  // namespace base

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
    on_tick(float dur_sec);

    gpu::camera_data
    get_camera_data();

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
    ev_mouse_press();

    void
    ev_reload();

    void
    ev_spawn();

    void
    ev_spawn2();

    void
    ev_lights();

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

    base::camera_component*
    get_active_camera() const;

private:
    glm::mat4
    get_rotation_matrix();

    glm::vec3 m_position;
    glm::vec3 m_velocity;

    float m_pitch = -12.f;
    float m_yaw = 0.f;

    float m_forward_delta = 0.f;
    float m_left_delta = 0.f;
    float m_look_up_delta = 0.f;
    float m_look_left_delta = 0.f;
    float m_cached_aspect_ratio = 0.f;
    gpu::camera_data m_camera_data;

    bool m_updated = true;

    editor_mode m_mode = editor_mode::editor;
    glm::vec3 m_saved_position;
    float m_saved_pitch = 0.f;
    float m_saved_yaw = 0.f;
    bool m_saved_grid_visible = true;

    base::camera_component* m_active_camera = nullptr;
    base::input_component* m_input = nullptr;
};
}  // namespace engine
namespace glob
{
struct game_editor : public singleton_instance<::kryga::engine::game_editor, game_editor>
{
};
}  // namespace glob

}  // namespace kryga