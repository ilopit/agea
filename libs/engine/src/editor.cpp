#include "engine/editor.h"

#include "engine/input_manager.h"

namespace agea::engine
{
void
game_editor::init()
{
    m_camera_data = {};
    position = {20.f, 6.f, 60.f};

    glob::input_manager::get()->register_scaled_action(AID("move_forward"), this,
                                                       &game_editor::ev_move_forward);
    glob::input_manager::get()->register_scaled_action(AID("move_left"), this,
                                                       &game_editor::ev_move_left);
    glob::input_manager::get()->register_scaled_action(AID("look_up"), this,
                                                       &game_editor::ev_look_up);
    glob::input_manager::get()->register_scaled_action(AID("look_left"), this,
                                                       &game_editor::ev_look_left);
}

void
game_editor::ev_move_forward(float f)
{
    forward_delta = f;
}

void
game_editor::ev_move_left(float f)
{
    left_delta = f;
}

void
game_editor::ev_look_up(float f)
{
    look_up_delta = f;
}

void
game_editor::ev_look_left(float f)
{
    look_left_delta = f;
}

glm::mat4
game_editor::get_rotation_matrix()
{
    glm::mat4 yaw_rot = glm::rotate(glm::mat4{1}, glm::radians(yaw), {0, 1, 0});
    glm::mat4 pitch_rot = glm::rotate(yaw_rot, glm::radians(pitch), {1, 0, 0});

    return pitch_rot;
}

void
game_editor::on_tick(float dt)
{
    update_camera();
}

void
game_editor::update_camera()
{
    glm::vec3 forward{0, 0, -1};
    glm::vec3 right{1, 0, 0};

    if (glob::input_manager::get()->get_input_state(agea::engine::ieid_ms_btm_right))
    {
        yaw += look_left_delta;
        pitch += look_up_delta;

        pitch = glm::clamp(pitch, -85.f, 85.f);
    }

    glm::mat4 cam_rot = get_rotation_matrix();

    forward = cam_rot * glm::vec4(forward, 0.f);
    right = cam_rot * glm::vec4(right, 0.f);

    velocity = forward_delta * forward + left_delta * right;
    position += velocity;

    forward_delta = 0.f;
    left_delta = 0.f;
    look_left_delta = 0.f;
    look_up_delta = 0.f;

    glm::mat4 view = glm::translate(glm::mat4{1}, position) * cam_rot;

    view = glm::inverse(view);

    glm::mat4 projection = glm::perspective(glm::radians(60.f), 16.f / 9.f, 0.1f, 2000.f);
    projection[1][1] *= -1;

    m_camera_data.projection = projection;
    m_camera_data.view = view;
    m_camera_data.position = position;
}

agea::render::gpu_camera_data
game_editor::get_camera_data()
{
    return m_camera_data;
}

}  // namespace agea::engine