#include "packages/base/model/camera_object.h"
#include "packages/root/model/game_object.h"

#include "packages/base/model/components/camera_component.h"
#include "packages/base/model/components/input_component.h"

#include <core/input_provider.h>

namespace kryga
{
namespace base
{

KRG_gen_class_cd_default(camera_object);

bool
camera_object::construct(this_class::construct_params& p)
{
    if (!base_class::construct(p))
    {
        return false;
    }

    camera_component::construct_params ccp;
    m_camera_component =
        spawn_component<camera_component>(get_root_component(), AID("camera"), ccp);

    input_component::construct_params cicp;
    m_camera_input_component =
        spawn_component<input_component>(get_root_component(), AID("camera_input"), cicp);

    return true;
}

void
camera_object::begin_play()
{
    auto* ip = m_camera_input_component->get_input_provider();
    if (!ip)
    {
        return;
    }

    ip->register_scaled_action(AID("move_forward"), this, &camera_object::on_move_forward);
    ip->register_scaled_action(AID("move_left"), this, &camera_object::on_move_left);
    ip->register_scaled_action(AID("look_up"), this, &camera_object::on_look_up);
    ip->register_scaled_action(AID("look_left"), this, &camera_object::on_look_left);
}

void
camera_object::end_play()
{
    auto* ip = m_camera_input_component->get_input_provider();
    if (!ip)
    {
        return;
    }

    ip->unregister_owner(this);

    m_forward_delta = 0.f;
    m_left_delta = 0.f;
    m_look_up_delta = 0.f;
    m_look_left_delta = 0.f;
}

void
camera_object::on_tick(float dt)
{
    auto* ip = m_camera_input_component->get_input_provider();

    if (ip && ip->get_input_state(core::mouse_right))
    {
        m_yaw += m_look_left_delta;
        m_pitch += m_look_up_delta;
        m_pitch = glm::clamp(m_pitch, -85.f, 85.f);
    }

    glm::mat4 yaw_rot = glm::rotate(glm::mat4{1}, glm::radians(m_yaw), {0, 1, 0});
    glm::mat4 cam_rot = glm::rotate(yaw_rot, glm::radians(m_pitch), {1, 0, 0});

    glm::vec3 forward{0, 0, -1};
    glm::vec3 right{1, 0, 0};

    forward = cam_rot * glm::vec4(forward, 0.f);
    right = cam_rot * glm::vec4(right, 0.f);

    glm::vec3 velocity = m_forward_delta * forward + m_left_delta * right;

    glm::vec3 pos = get_position();
    pos += velocity;
    set_position(pos);

    set_rotation(root::vec3{m_pitch, m_yaw, 0.f});

    m_forward_delta = 0.f;
    m_left_delta = 0.f;
    m_look_up_delta = 0.f;
    m_look_left_delta = 0.f;
}

void
camera_object::on_move_forward(float v)
{
    m_forward_delta = v;
}

void
camera_object::on_move_left(float v)
{
    m_left_delta = v;
}

void
camera_object::on_look_up(float v)
{
    m_look_up_delta = v;
}

void
camera_object::on_look_left(float v)
{
    m_look_left_delta = v;
}

}  // namespace base
}  // namespace kryga
