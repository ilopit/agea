#include "engine/editor.h"

#include "engine/input_manager.h"
#include "engine/agea_engine.h"

#include <native/native_window.h>
#include <vulkan_render/agea_render.h>

#include <core/level.h>
#include <core/level_manager.h>
#include <core/package_manager.h>
#include <root/game_object.h>
#include <root/lights/point_light.h>
#include <root/lights/directional_light.h>
#include <root/lights/spot_light.h>

namespace agea
{
glob::game_editor::type glob::game_editor::type::s_instance;
namespace engine
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

    glob::input_manager::get()->register_fixed_action(AID("level_reload"), true, this,
                                                      &game_editor::ev_reload);

    glob::input_manager::get()->register_fixed_action(AID("spawn"), true, this,
                                                      &game_editor::ev_spawn);

    glob::input_manager::get()->register_fixed_action(AID("lights"), true, this,
                                                      &game_editor::ev_lights);

    glob::input_manager::get()->register_fixed_action(AID("mouse_pressed"), true, this,
                                                      &game_editor::ev_mouse_press);
}

void
game_editor::ev_move_forward(float f)
{
    forward_delta = f;
    m_updated = true;
}

void
game_editor::ev_move_left(float f)
{
    left_delta = f;
    m_updated = true;
}

void
game_editor::ev_look_up(float f)
{
    look_up_delta = f;
    m_updated = true;
}

void
game_editor::ev_look_left(float f)
{
    look_left_delta = f;
    m_updated = true;
}

void
game_editor::ev_mouse_press()
{
    uint32_t w = glob::input_manager::getr().get_mouse_state().x;
    uint32_t h = glob::input_manager::getr().get_mouse_state().y;

    auto obj = glob::vulkan_render::getr().object_id_under_coordinate(w, h);
    if (obj)
    {
        obj->outlined = !obj->outlined;
        glob::vulkan_render::getr().reschedule_to_drawing(obj);
    }
}

void
game_editor::ev_reload()
{
    auto& level = glob::level::getr();

    level.drop_pending_updates();

    glob::vulkan_render::getr().clear_upload_queue();

    auto pids = level.get_package_ids();

    glob::engine::getr().unload_render_resources(level);
    glob::level_manager::getr().unload_level(level);

    auto& pm = glob::package_manager::getr();
    for (auto& id : pids)
    {
        auto p = pm.get_package(id);
        glob::engine::getr().unload_render_resources(*p);
        pm.unload_package(*p);
    }

    glob::engine::getr().init_scene();
}

void
game_editor::ev_spawn()
{
    if (glob::level::getr().find_game_object(AID("obj_0_0_0")))
    {
        return;
    }

    core::spawn_parameters sp;
    auto id1 = AID("decor");
    auto id2 = AID("decor");

    int x = 0, y = 0, z = 0;

    int DIM = 0;

    for (x = 0; x < DIM; ++x)
    {
        for (y = 0; y < DIM; ++y)
        {
            for (z = 0; z < DIM; ++z)
            {
                auto id = std::format("obj_{}_{}_{}", x, y, z);

                sp.positon = root::vec3{x * 40.f, y * 40.f, z * 40.f};
                sp.scale = root::vec3{10.f};
                auto p = glob::level::getr().spawn_object_from_proto<root::game_object>(
                    (z & 1) ? id1 : id2, AID(id), sp);
            }
        }
    }
}

void
game_editor::ev_lights()
{
    if (glob::level::getr().find_game_object(AID("PL1")))
    {
        return;
    }

    {
        root::spot_light::construct_params plp;
        plp.pos = {-20.f};
        glob::level::getr().spawn_object<root::spot_light>(AID("PL1"), plp);
    }

    {
        root::point_light::construct_params plp;
        plp.pos = {15.f};
        glob::level::getr().spawn_object<root::point_light>(AID("PL2"), plp);
    }

    {
        root::directional_light::construct_params dcp;
        dcp.pos = {0.f, 20.f, 0.0};
        glob::level::getr().spawn_object<root::directional_light>(AID("DL"), dcp);
    }
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
    if (!m_updated)
    {
        return;
    }

    if (glob::input_manager::get()->get_input_state(agea::engine::mouse_right))
    {
        yaw += look_left_delta;
        pitch += look_up_delta;

        pitch = glm::clamp(pitch, -85.f, 85.f);
    }

    glm::mat4 cam_rot = get_rotation_matrix();

    glm::vec3 forward{0, 0, -1};
    glm::vec3 right{1, 0, 0};

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

    glm::mat4 projection = glm::perspective(
        glm::radians(60.f), glob::native_window::getr().aspect_ratio(), 0.1f, 2000.f);
    projection[1][1] *= -1;

    m_camera_data.projection = projection;
    m_camera_data.view = view;
    m_camera_data.position = position;

    m_updated = false;
}

render::gpu_camera_data
game_editor::get_camera_data()
{
    return m_camera_data;
}

}  // namespace engine

}  // namespace agea