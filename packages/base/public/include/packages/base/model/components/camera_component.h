#pragma once

#include "packages/base/model/camera_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

namespace kryga
{
namespace base
{
KRG_ar_class();
class camera_component : public ::kryga::root::game_object_component
{
    KRG_gen_meta__camera_component();

public:
    KRG_gen_class_meta(camera_component, game_object_component);
    KRG_gen_construct_params
    {
        float rotation_speed = 1.0f;
        float movement_speed = 0.01f;

        float fov = 60.f;
        float znear = 0.1f;
        float zfar = 256.0f;

        float aspect_ratio = 16.0f / 9.0f;
        glm::mat4 scale = glm::mat4(1.f);

        bool is_active_camera = false;
    };
    KRG_gen_meta_api;

    bool
    construct(construct_params& c);

    void
    on_tick(float dt) override;

    float
    near_clip() const;
    float
    far_clip() const;

    void
    set_perspective(float fov, float aspect, float znear, float zfar);
    void
    set_aspect_ratio(float aspect);

    void
    set_rotation_speed(float rs);
    void
    set_movement_speed(float ms);

    const glm::mat4&
    get_perspective()
    {
        return m_perspective;
    }

    const glm::mat4&
    get_view()
    {
        return m_view;
    }

    float
    get_rotation_speed()
    {
        return m_rotation_speed;
    }

    float
    get_movement_speed()
    {
        return m_movement_speed;
    }

    float
    aspect_ratio()
    {
        return m_aspect_ratio;
    }

    bool
    is_active_camera() const
    {
        return m_is_active_camera;
    }

    void
    set_active_camera(bool v)
    {
        m_is_active_camera = v;
    }

    const glm::mat4&
    get_inv_projection()
    {
        return m_inv_projection;
    }

    void
    update_model();

    void
    update_view();

    void
    update_perspective();

private:
    // Updateble part
    float m_rotation_speed = 1.0f;
    float m_movement_speed = 0.01f;

    float m_fov = 0.f;
    float m_znear = 0.f;
    float m_zfar = 0.f;

    float m_aspect_ratio = 16.0f / 9.0f;
    glm::mat4 m_scale;

    KRG_ar_property("category=Camera", "serializable=true", "default=true");
    bool m_is_active_camera = false;

    // Internal state
private:
    glm::mat4 m_perspective;
    glm::mat4 m_inv_projection;
    glm::mat4 m_view;
    glm::mat4 m_model;
};
}  // namespace base
}  // namespace kryga
