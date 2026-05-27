#include "packages/base/model/player.h"

#include "packages/base/model/components/camera_component.h"
#include "packages/base/model/components/input_component.h"

#include <core/input_provider.h>
#include <core/model_system.h>
#include <global_state/global_state.h>

#include <glm_unofficial/glm.h>

#include <cmath>

namespace kryga
{
namespace base
{

KRG_gen_class_cd_default(player);

namespace
{

glm::vec3
snap_to_axis(const glm::vec3& v)
{
    glm::vec3 a = glm::abs(v);
    if (a.x >= a.y && a.x >= a.z)
    {
        return glm::vec3(v.x > 0.f ? 1.f : -1.f, 0.f, 0.f);
    }
    if (a.y >= a.x && a.y >= a.z)
    {
        return glm::vec3(0.f, v.y > 0.f ? 1.f : -1.f, 0.f);
    }
    return glm::vec3(0.f, 0.f, v.z > 0.f ? 1.f : -1.f);
}

glm::quat
canonical_orbit(const glm::vec3& offset_dir, const glm::quat& from)
{
    glm::vec3 world_up(0.f, 1.f, 0.f);
    float pole = std::abs(glm::dot(offset_dir, world_up));

    glm::vec3 right;
    glm::vec3 up;

    if (pole < 0.99f)
    {
        right = glm::normalize(glm::cross(world_up, offset_dir));
        up = glm::cross(offset_dir, right);
    }
    else
    {
        glm::vec3 from_fwd = from * glm::vec3(0.f, 0.f, 1.f);
        glm::vec3 up_hint = from_fwd - glm::dot(from_fwd, offset_dir) * offset_dir;

        if (glm::length(up_hint) < 0.01f)
        {
            up_hint = glm::vec3(0.f, 0.f, offset_dir.y > 0.f ? -1.f : 1.f);
        }
        else
        {
            up_hint = glm::normalize(up_hint);
        }

        right = glm::normalize(glm::cross(up_hint, offset_dir));
        up = glm::cross(offset_dir, right);

        glm::vec3 from_right = from * glm::vec3(1.f, 0.f, 0.f);
        if (glm::dot(right, from_right) < 0.f)
        {
            right = -right;
            up = -up;
        }
    }

    glm::mat3 m;
    m[0] = right;
    m[1] = up;
    m[2] = offset_dir;
    return glm::normalize(glm::quat_cast(m));
}

// Returns true while still animating.
bool
spring_quat(glm::quat& ori, const glm::quat& target_ori, glm::vec3& ang_vel,
            float stiffness, float damping, float dt)
{
    glm::quat delta = target_ori * glm::inverse(ori);
    if (delta.w < 0.f)
    {
        delta = -delta;
    }

    float half_angle = std::acos(glm::clamp(delta.w, -1.f, 1.f));
    float angle = 2.f * half_angle;

    glm::vec3 displacement(0.f);
    if (angle > 0.001f)
    {
        glm::vec3 axis = glm::normalize(glm::vec3(delta.x, delta.y, delta.z));
        displacement = axis * angle;
    }

    glm::vec3 force = stiffness * displacement - damping * ang_vel;
    ang_vel += force * dt;

    float omega = glm::length(ang_vel);
    if (omega > 0.001f)
    {
        glm::quat spin = glm::angleAxis(omega * dt, ang_vel / omega);
        ori = glm::normalize(spin * ori);
    }

    return angle > glm::radians(0.5f) || omega > 0.1f;
}

}  // namespace

bool
player::construct(this_class::construct_params& p)
{
    if (!base_class::construct(p))
    {
        return false;
    }

    m_orbit_center = p.orbit_center;
    m_orbit_radius = p.orbit_radius;
    m_min_radius = p.min_radius;
    m_max_radius = p.max_radius;
    m_zoom_speed = p.zoom_speed;
    m_spring_stiffness = p.spring_stiffness;
    m_spring_damping = p.spring_damping;
    m_mode = p.mode;

    camera_component::construct_params ccp;
    m_camera = spawn_component<camera_component>(get_root_component(), AID("camera"), ccp);

    input_component::construct_params icp;
    m_input = spawn_component<input_component>(get_root_component(), AID("input"), icp);

    return true;
}

void
player::begin_play()
{
    auto* ip = m_input->get_input_provider();
    if (!ip)
    {
        return;
    }

    ip->register_scaled_action(AID("move_forward"), this, &player::on_move_forward);
    ip->register_scaled_action(AID("move_left"), this, &player::on_move_right);
    ip->register_scaled_action(AID("zoom"), this, &player::on_zoom);
    ip->register_scaled_action(AID("look_left"), this, &player::on_look_x);
    ip->register_scaled_action(AID("look_up"), this, &player::on_look_y);

    m_ori = m_target_ori;
    m_ang_vel = glm::vec3(0.f);
    m_cube_ori = m_cube_target_ori;
    m_cube_ang_vel = glm::vec3(0.f);

    if (!m_target)
    {
        auto* lvl = glob::glob_state().getr_model().current_level;
        if (lvl)
        {
            m_target = lvl->find_game_object(AID("cubes_root"));
        }
    }
}

void
player::end_play()
{
    auto* ip = m_input->get_input_provider();
    if (!ip)
    {
        return;
    }

    ip->unregister_owner(this);

    m_forward_input = 0.f;
    m_right_input = 0.f;
    m_zoom_input = 0.f;
    m_look_x_input = 0.f;
    m_look_y_input = 0.f;
    m_rotating = false;
    m_cube_rotating = false;
}

void
player::on_tick(float dt)
{
    if (m_mode == orbit_mode::snap_orbit)
    {
        tick_snap_orbit(dt);
    }
    else
    {
        tick_mouse_orbit(dt);
    }
}

void
player::tick_snap_orbit(float dt)
{
    if (m_rotating)
    {
        glm::quat diff = m_target_ori * glm::inverse(m_ori);
        if (diff.w < 0.f)
        {
            diff = -diff;
        }
        float remaining = 2.f * std::acos(glm::clamp(diff.w, -1.f, 1.f));
        if (remaining < glm::radians(2.f) && glm::length(m_ang_vel) < 0.3f)
        {
            m_rotating = false;
        }
    }

    if (!m_rotating)
    {
        bool triggered = false;

        auto apply_rotation = [&](const glm::vec3& axis, float degrees)
        {
            glm::quat raw =
                glm::normalize(glm::angleAxis(glm::radians(degrees), axis) * m_target_ori);
            glm::vec3 new_dir = snap_to_axis(raw * glm::vec3(0, 0, 1));
            m_target_ori = canonical_orbit(new_dir, m_target_ori);
            triggered = true;
        };

        if (m_forward_input > 0.f)
        {
            apply_rotation(m_target_ori * glm::vec3(1, 0, 0), -90.f);
        }
        else if (m_forward_input < 0.f)
        {
            apply_rotation(m_target_ori * glm::vec3(1, 0, 0), 90.f);
        }

        if (m_right_input > 0.f)
        {
            apply_rotation(m_target_ori * glm::vec3(0, 1, 0), -90.f);
        }
        else if (m_right_input < 0.f)
        {
            apply_rotation(m_target_ori * glm::vec3(0, 1, 0), 90.f);
        }

        if (triggered)
        {
            m_rotating = true;
        }
    }

    m_forward_input = 0.f;
    m_right_input = 0.f;

    if (std::fabs(m_zoom_input) > 0.01f)
    {
        m_orbit_radius += m_zoom_input * m_zoom_speed * dt;
        m_orbit_radius = glm::clamp(m_orbit_radius, m_min_radius, m_max_radius);
        m_zoom_input = 0.f;
    }

    spring_quat(m_ori, m_target_ori, m_ang_vel, m_spring_stiffness, m_spring_damping, dt);

    glm::vec3 offset = m_ori * glm::vec3(0, 0, m_orbit_radius);
    glm::vec3 cam_pos = m_orbit_center + offset;

    set_position(root::vec3{cam_pos.x, cam_pos.y, cam_pos.z});
    m_camera->set_look_at(m_orbit_center, m_ori * glm::vec3(0, 1, 0));
}

void
player::tick_mouse_orbit(float dt)
{
    auto* ip = m_input->get_input_provider();
    bool mouse_held = ip && ip->get_input_state(core::mouse_left);

    if (mouse_held)
    {
        constexpr float k_sensitivity = 3.f;
        m_yaw += m_look_x_input * k_sensitivity;
        m_pitch += m_look_y_input * k_sensitivity;
        m_pitch = glm::clamp(m_pitch, -85.f, 85.f);
    }

    m_look_x_input = 0.f;
    m_look_y_input = 0.f;

    if (!m_cube_rotating)
    {
        bool triggered = false;

        auto apply_cube_rotation = [&](const glm::vec3& world_axis, float degrees)
        {
            glm::quat rot = glm::angleAxis(glm::radians(degrees), world_axis);
            m_cube_target_ori = glm::normalize(rot * m_cube_target_ori);
            triggered = true;
        };

        if (m_forward_input > 0.f)
        {
            apply_cube_rotation(glm::vec3(1, 0, 0), -90.f);
        }
        else if (m_forward_input < 0.f)
        {
            apply_cube_rotation(glm::vec3(1, 0, 0), 90.f);
        }

        if (m_right_input > 0.f)
        {
            apply_cube_rotation(glm::vec3(0, 1, 0), -90.f);
        }
        else if (m_right_input < 0.f)
        {
            apply_cube_rotation(glm::vec3(0, 1, 0), 90.f);
        }

        if (triggered)
        {
            m_cube_rotating = true;
        }
    }

    m_forward_input = 0.f;
    m_right_input = 0.f;

    if (m_cube_rotating)
    {
        bool still_moving = spring_quat(m_cube_ori,
                                        m_cube_target_ori,
                                        m_cube_ang_vel,
                                        m_spring_stiffness,
                                        m_spring_damping,
                                        dt);
        if (!still_moving)
        {
            m_cube_rotating = false;
        }

        if (m_target)
        {
            glm::vec3 euler = glm::degrees(glm::eulerAngles(m_cube_ori));
            m_target->set_rotation(root::vec3{euler.x, euler.y, euler.z});
        }
        else
        {
            glm::vec3 offset_dir = glm::normalize(m_cube_ori * glm::vec3(0, 0, 1));
            m_yaw = glm::degrees(std::atan2(offset_dir.x, offset_dir.z));
            m_pitch = glm::clamp(
                glm::degrees(std::asin(glm::clamp(offset_dir.y, -1.f, 1.f))), -85.f, 85.f);
        }
    }

    if (std::fabs(m_zoom_input) > 0.01f)
    {
        m_orbit_radius += m_zoom_input * m_zoom_speed * dt;
        m_orbit_radius = glm::clamp(m_orbit_radius, m_min_radius, m_max_radius);
        m_zoom_input = 0.f;
    }

    float yaw_rad = glm::radians(m_yaw);
    float pitch_rad = glm::radians(m_pitch);

    glm::vec3 offset;
    offset.x = std::cos(pitch_rad) * std::sin(yaw_rad);
    offset.y = std::sin(pitch_rad);
    offset.z = std::cos(pitch_rad) * std::cos(yaw_rad);

    glm::vec3 cam_pos = m_orbit_center + offset * m_orbit_radius;

    set_position(root::vec3{cam_pos.x, cam_pos.y, cam_pos.z});
    m_camera->set_look_at(m_orbit_center);
}

void
player::on_move_forward(float v)
{
    m_forward_input = v;
}

void
player::on_move_right(float v)
{
    m_right_input = v;
}

void
player::on_look_x(float v)
{
    m_look_x_input = v;
}

void
player::on_look_y(float v)
{
    m_look_y_input = v;
}

void
player::on_zoom(float v)
{
    m_zoom_input = v;
}

}  // namespace base
}  // namespace kryga
