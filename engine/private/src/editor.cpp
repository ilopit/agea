#include "engine/editor.h"

#include "engine/input_manager.h"
#include "engine/agea_engine.h"

#include <native/native_window.h>
#include <vulkan_render/agea_render.h>

#include <core/level.h>
#include <core/level_manager.h>
#include <core/package_manager.h>
#include <core/global_state.h>

#include <packages/root/model/game_object.h>
#include <packages/base/model/mesh_object.h>
#include <packages/base/model/components/mesh_component.h>
#include <packages/base/model/lights/point_light.h>
#include <packages/base/model/lights/directional_light.h>
#include <packages/base/model/lights/spot_light.h>

namespace agea
{
glob::game_editor::type glob::game_editor::type::s_instance;
namespace engine
{

void
game_editor::init()
{
    m_camera_data = {};
    m_position = {20.f, 6.f, 60.f};

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
    m_forward_delta = f;
    m_updated = true;
}

void
game_editor::ev_move_left(float f)
{
    m_left_delta = f;
    m_updated = true;
}

void
game_editor::ev_look_up(float f)
{
    m_look_up_delta = f;
    m_updated = true;
}

void
game_editor::ev_look_left(float f)
{
    m_look_left_delta = f;
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
    auto& level = glob::glob_state().getr_current_level();

    level.drop_pending_updates();

    glob::vulkan_render::getr().clear_upload_queue();

    auto pids = level.get_package_ids();

    glob::engine::getr().unload_render_resources(level);

    auto lm = glob::glob_state().get_lm();
    auto pm = glob::glob_state().get_pm();

    lm->unload_level(level);

    for (auto& id : pids)
    {
        auto p = pm->get_package(id);
        glob::engine::getr().unload_render_resources(*p);
        pm->unload_package(*p);
    }

    glob::engine::getr().init_scene();
}

void
game_editor::ev_spawn()
{
    if (glob::glob_state().getr_current_level().find_game_object(AID("obj_0_0_0")))
    {
        return;
    }

    core::spawn_parameters sp;

    int x = 0, y = 0, z = 0;

    int obj_DIM = 10;

    for (x = 0; x < obj_DIM; ++x)
    {
        for (y = 0; y < obj_DIM; ++y)
        {
            for (z = 0; z < obj_DIM; ++z)
            {
                auto id = std::format("obj_{}_{}_{}", x, y, z);

                sp.position = root::vec3{x * 10.f, y * 10.f, z * 10.f};
                sp.scale = root::vec3{2};
                auto pp =
                    glob::glob_state()
                        .getr_current_level()
                        .spawn_object_as_clone<base::mesh_object>(AID("test_cube"), AID(id), sp);
                auto mc = pp->get_component_at(1)->as<base::mesh_component>();
                mc->set_visible(true);
            }
        }
    }

    int light_DIM = 2;

    base::point_light::construct_params prms;

    for (x = 0; x < light_DIM; ++x)
    {
        for (y = 0; y < light_DIM; ++y)
        {
            for (z = 0; z < light_DIM; ++z)
            {
                auto id = std::format("pl_{}_{}_{}", x, y, z);

                prms.pos = root::vec3{x * 20.f, y * 20.f, z * 20.f};
                auto pp = glob::glob_state().getr_current_level().spawn_object<base::point_light>(
                    AID(id), prms);
                int i = 2;
            }
        }
    }
}

void
game_editor::ev_lights()
{
    auto& lvl = glob::glob_state().getr_current_level();

    if (lvl.find_game_object(AID("PL1")))
    {
        return;
    }

    {
        base::spot_light::construct_params plp;
        plp.pos = {-20.f};
        lvl.spawn_object<base::spot_light>(AID("PL1"), plp);
    }

    {
        base::point_light::construct_params plp;
        plp.pos = {15.f};
        lvl.spawn_object<base::point_light>(AID("PL2"), plp);
    }

    {
        base::directional_light::construct_params dcp;
        dcp.pos = {0.f, 20.f, 0.0};
        lvl.spawn_object<base::directional_light>(AID("DL"), dcp);
    }
}

glm::mat4
game_editor::get_rotation_matrix()
{
    glm::mat4 yaw_rot = glm::rotate(glm::mat4{1}, glm::radians(m_yaw), {0, 1, 0});
    glm::mat4 pitch_rot = glm::rotate(yaw_rot, glm::radians(m_pitch), {1, 0, 0});

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
        m_yaw += m_look_left_delta;
        m_pitch += m_look_up_delta;

        m_pitch = glm::clamp(m_pitch, -85.f, 85.f);
    }

    glm::mat4 cam_rot = get_rotation_matrix();

    glm::vec3 forward{0, 0, -1};
    glm::vec3 right{1, 0, 0};

    forward = cam_rot * glm::vec4(forward, 0.f);
    right = cam_rot * glm::vec4(right, 0.f);

    m_velocity = m_forward_delta * forward + m_left_delta * right;
    m_position += m_velocity;

    m_forward_delta = 0.f;
    m_left_delta = 0.f;
    m_look_left_delta = 0.f;
    m_look_up_delta = 0.f;

    glm::mat4 view = glm::translate(glm::mat4{1}, m_position) * cam_rot;

    view = glm::inverse(view);

    glm::mat4 projection = glm::perspective(
        glm::radians(60.f), glob::native_window::getr().aspect_ratio(), 0.1f, 2000.f);
    projection[1][1] *= -1;

    m_camera_data.projection = projection;
    m_camera_data.view = view;
    m_camera_data.position = m_position;

    m_updated = false;
}

render::gpu_camera_data
game_editor::get_camera_data()
{
    return m_camera_data;
}

}  // namespace engine

}  // namespace agea