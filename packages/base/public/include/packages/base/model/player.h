#pragma once

#include "packages/base/model/player.ar.h"

#include "packages/root/model/game_object.h"

#include <glm_unofficial/glm.h>

namespace kryga
{
namespace base
{

class camera_component;
class input_component;

enum class orbit_mode
{
    snap_orbit,
    mouse_orbit,
};

// clang-format off
KRG_ar_class(
    mcp_hint = "Player with orbit camera: snap_orbit - WASD rotates camera 90deg, mouse_orbit - mouse drags camera, WASD rotates target cube"
);
class player : public ::kryga::root::game_object
// clang-format on
{
    KRG_gen_meta__player();

public:
    KRG_gen_class_meta(player, ::kryga::root::game_object);
    KRG_gen_meta_api;

    KRG_gen_construct_params
    {
        root::vec3 orbit_center = {0.f, 0.f, 0.f};
        float orbit_radius = 10.f;
        float min_radius = 2.f;
        float max_radius = 100.f;
        float zoom_speed = 2.f;
        float spring_stiffness = 9.f;
        float spring_damping = 4.f;
        orbit_mode mode = orbit_mode::mouse_orbit;
    };

    camera_component*
    get_camera() const
    {
        return m_camera;
    }

    input_component*
    get_input() const
    {
        return m_input;
    }

    void
    set_target(root::game_object* t)
    {
        m_target = t;
    }

    void
    set_orbit_mode(orbit_mode m)
    {
        m_mode = m;
    }

    void
    begin_play() override;
    void
    end_play() override;
    void
    on_tick(float dt) override;

protected:
    bool
    construct(this_class::construct_params& p);

private:
    void
    tick_snap_orbit(float dt);
    void
    tick_mouse_orbit(float dt);

    void
    on_move_forward(float v);
    void
    on_move_right(float v);
    void
    on_look_x(float v);
    void
    on_look_y(float v);
    void
    on_zoom(float v);

    camera_component* m_camera = nullptr;
    input_component* m_input = nullptr;
    root::game_object* m_target = nullptr;
    orbit_mode m_mode = orbit_mode::mouse_orbit;

    glm::vec3 m_orbit_center{0.f};
    float m_orbit_radius = 10.f;
    float m_min_radius = 2.f;
    float m_max_radius = 100.f;
    float m_zoom_speed = 2.f;

    // Variant A: snap orbit (quaternion-based camera rotation)
    glm::quat m_ori{1.f, 0.f, 0.f, 0.f};
    glm::quat m_target_ori{1.f, 0.f, 0.f, 0.f};
    glm::vec3 m_ang_vel{0.f};
    bool m_rotating = false;

    // Variant B: mouse orbit (yaw/pitch camera + cube rotation)
    float m_yaw = 0.f;
    float m_pitch = 25.f;
    float m_look_x_input = 0.f;
    float m_look_y_input = 0.f;
    glm::quat m_cube_ori{1.f, 0.f, 0.f, 0.f};
    glm::quat m_cube_target_ori{1.f, 0.f, 0.f, 0.f};
    glm::vec3 m_cube_ang_vel{0.f};
    bool m_cube_rotating = false;

    float m_spring_stiffness = 9.f;
    float m_spring_damping = 4.f;

    float m_forward_input = 0.f;
    float m_right_input = 0.f;
    float m_zoom_input = 0.f;
};

}  // namespace base
}  // namespace kryga
