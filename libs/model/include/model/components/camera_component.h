#pragma once

#include "model/components/game_object_component.h"

#include "model/camera_component.generated.h"

namespace agea
{
namespace model
{
AGEA_ar_class();
class camera_component : public game_object_component
{
    AGEA_gen_meta__camera_component();

public:
    AGEA_gen_class_meta(camera_component, game_object_component);
    AGEA_gen_construct_params
    {
        float rotation_speed = 1.0f;
        float movement_speed = 0.01f;

        float fov = 60.f;
        float znear = 0.1f;
        float zfar = 256.0f;

        float aspect_ratio = 16.0f / 9.0f;
        glm::mat4 scale = glm::mat4(1.f);
    };
    AGEA_gen_meta_api;

    bool
    construct(construct_params& c);

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

    // Internal state
private:
    glm::mat4 m_perspective;
    glm::mat4 m_view;
    glm::mat4 m_model;
};
}  // namespace model
}  // namespace agea
