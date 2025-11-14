#include "packages/base/model/components/camera_component.h"

#include "packages/root/model/game_object.h"

namespace agea
{
namespace base
{

AGEA_gen_class_cd_default(camera_component);

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
    //     auto owner_obj = (game_object*)get_owner();
    //     m_view = glm::lookAt(owner_obj->get_position(),
    //                          owner_obj->get_position() + get_forward_vector(), get_up_vector());
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
}  // namespace agea
