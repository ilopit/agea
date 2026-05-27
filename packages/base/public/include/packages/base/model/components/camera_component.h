#pragma once

#include "packages/base/model/camera_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

namespace kryga
{
namespace base
{

enum class camera_mode
{
    free,
    look_at,
};

// clang-format off
KRG_ar_class(
    mcp_hint = "Camera projection and viewport control — FOV / near/far planes / aspect ratio / "
               "active camera flag"
);
class camera_component : public ::kryga::root::game_object_component
// clang-format on
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

    void
    set_look_at(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.f, 1.f, 0.f));

    void
    set_free();

private:
    camera_mode m_mode = camera_mode::free;
    glm::vec3 m_look_at_target{0.f};
    glm::vec3 m_look_at_up{0.f, 1.f, 0.f};
    // Updateble part
    float m_rotation_speed = 1.0f;
    float m_movement_speed = 0.01f;

    float m_fov = 0.f;
    float m_znear = 0.f;
    float m_zfar = 0.f;

    float m_aspect_ratio = 16.0f / 9.0f;
    glm::mat4 m_scale;

    // clang-format off
    KRG_ar_property(
        category     = "Camera",
        serializable = true,
        default      = true,
        mcp_hint     = "whether this camera is the active viewport camera"
    );
    bool m_is_active_camera = false;
    // clang-format on

    // Internal state
private:
    glm::mat4 m_perspective;
    glm::mat4 m_inv_projection;
    glm::mat4 m_view;
    glm::mat4 m_model;
};
}  // namespace base
}  // namespace kryga
