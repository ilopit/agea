#pragma once

#include <vulkan_render/types/vulkan_gpu_types.h>

#include <utils/singleton_instance.h>

namespace agea
{
namespace engine
{
class game_editor
{
public:
    void
    init();

    void
    on_tick(float dur_sec);

    render::gpu_camera_data
    get_camera_data();

private:
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

    glm::mat4
    get_rotation_matrix();

    glm::vec3 position;
    glm::vec3 velocity;

    float pitch = -12.f;
    float yaw = 0.f;

    float forward_delta = 0.f;
    float left_delta = 0.f;
    float look_up_delta = 0.f;
    float look_left_delta = 0.f;

    render::gpu_camera_data m_camera_data;

    bool m_updated = true;
};
}  // namespace engine
namespace glob
{
struct game_editor : public singleton_instance<::agea::engine::game_editor, game_editor>
{
};
}  // namespace glob

}  // namespace agea