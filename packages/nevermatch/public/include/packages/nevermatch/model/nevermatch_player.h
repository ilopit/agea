#pragma once

#include "packages/nevermatch/model/nevermatch_player.ar.h"

#include "packages/root/model/player.h"

#include <glm_unofficial/glm.h>

namespace kryga::nevermatch
{

enum class orbit_mode
{
    snap_orbit,
    mouse_orbit,
};

// The nevermatch player: an orbit-camera controller built on the generic
// root::player scaffold (which owns the camera + input components). Holds the
// game-specific control scheme — snap_orbit (WASD rotates camera 90deg) and
// mouse_orbit (mouse drags camera, WASD rotates the target cube).
// clang-format off
KRG_ar_class(
    mcp_hint = "Player with orbit camera: snap_orbit - WASD rotates camera 90deg, mouse_orbit - mouse drags camera, WASD rotates target cube"
);
class nevermatch_player : public ::kryga::root::player
// clang-format on
{
    KRG_gen_meta__nevermatch_player();

public:
    KRG_gen_class_meta(nevermatch_player, ::kryga::root::player);
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

    orbit_mode m_mode = orbit_mode::mouse_orbit;

    glm::vec3 m_orbit_center{0.f};

protected:
    // clang-format off
    KRG_ar_property(
        category     = "Camera",
        access       = all,
        serializable = true,
        default      = true,
        mcp_hint     = "orbit camera distance from the pivot - set it to move the "
                       "camera and listener nearer or farther from the scene"
    );
    float m_orbit_radius = 10.f;
    // clang-format on

private:
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

}  // namespace kryga::nevermatch
