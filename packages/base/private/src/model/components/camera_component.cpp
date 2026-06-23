#include "packages/base/model/components/camera_component.h"

#include "packages/root/model/game_object.h"

#include <core/level.h>

namespace kryga
{
namespace base
{

KRG_gen_class_cd_default(camera_component);

void
camera_component::set_active_camera(bool v)
{
    m_is_active_camera = v;

    // Register into THIS object's level (not glob current_level): this also runs at
    // load time (via post_load) before the level becomes current. The cast drops the
    // accessor's const — writing the level's active-camera bookkeeping is a legitimate
    // model mutation. Null for an object not attached to a level (e.g. a CDO).
    auto* lvl = const_cast<core::level*>(get_level());
    if (!lvl)
    {
        return;
    }

    if (v)
    {
        // Exclusivity: demote a different camera that held the active slot.
        const auto& prev = lvl->get_active_camera_id();
        if (prev.valid() && prev != get_id())
        {
            if (auto* pc = lvl->find_component(prev))
            {
                if (auto* pcc = pc->as<camera_component>())
                {
                    pcc->m_is_active_camera = false;
                }
            }
        }
        lvl->set_active_camera_id(get_id());
    }
    else if (lvl->get_active_camera_id() == get_id())
    {
        lvl->set_active_camera_id({});
    }
}

bool
camera_component::post_construct()
{
    if (!base_class::post_construct())
    {
        return false;
    }
    // Push the authored active flag into the level so find_active_camera resolves it
    // O(1) with no scene scan. Fires on every creation path (construct / load / clone),
    // after m_level and the properties are set.
    set_active_camera(m_is_active_camera);
    return true;
}

bool
camera_component::post_load()
{
    if (!base_class::post_load())
    {
        return false;
    }
    set_active_camera(m_is_active_camera);
    return true;
}

void
camera_component::set_rotation_speed(float rs)
{
    m_rotation_speed = rs;
}

void
camera_component::set_movement_speed(float ms)
{
    m_movement_speed = ms;
}

void
camera_component::update_view()
{
    auto owner_obj = get_owner();
    if (!owner_obj)
    {
        return;
    }

    glm::vec3 pos = owner_obj->get_position();

    if (m_mode == camera_mode::look_at)
    {
        m_view = glm::lookAt(pos, m_look_at_target, m_look_at_up);
        return;
    }

    glm::vec3 rot = owner_obj->get_rotation();
    glm::mat4 yaw_rot = glm::rotate(glm::mat4{1}, glm::radians(rot.y), {0, 1, 0});
    glm::mat4 cam_rot = glm::rotate(yaw_rot, glm::radians(rot.x), {1, 0, 0});

    m_view = glm::transpose(cam_rot) * glm::translate(glm::mat4{1.f}, -pos);
}

void
camera_component::update_model()
{
    //     auto owner_obj = (game_object*)get_owner();
    //     m_model = glm::translate(owner_obj->get_position()) *
    //               glm::toMat4(owner_obj->get_rotation_quat()) * m_scale;
}

void
camera_component::update_perspective()
{
    m_perspective = glm::perspective(glm::radians(m_fov), m_aspect_ratio, m_znear, m_zfar);
    m_perspective[1][1] *= -1;
    m_inv_projection = glm::inverse(m_perspective);
}

void
camera_component::on_tick(float dt)
{
    update_view();
    update_perspective();
}

void
camera_component::set_look_at(const glm::vec3& target, const glm::vec3& up)
{
    m_mode = camera_mode::look_at;
    m_look_at_target = target;
    m_look_at_up = up;
}

void
camera_component::set_free()
{
    m_mode = camera_mode::free;
}

bool
camera_component::construct(construct_params& c)
{
    base_class::construct(c);

    m_rotation_speed = c.rotation_speed;
    m_movement_speed = c.movement_speed;
    m_fov = c.fov;
    m_znear = c.znear;
    m_zfar = c.zfar;
    m_aspect_ratio = c.aspect_ratio;
    m_scale = c.scale;
    m_is_active_camera = c.is_active_camera;

    update_perspective();

    return true;
}

float
camera_component::near_clip() const
{
    return m_znear;
}

float
camera_component::far_clip() const
{
    return m_zfar;
}

void
camera_component::set_perspective(float fov, float aspect_ratio, float znear, float zfar)
{
    m_fov = fov;
    m_znear = znear;
    m_zfar = zfar;
    m_aspect_ratio = aspect_ratio;

    update_perspective();
}

void
camera_component::set_aspect_ratio(float aspect_ratio)
{
    m_aspect_ratio = aspect_ratio;

    update_perspective();
}

}  // namespace base
}  // namespace kryga
