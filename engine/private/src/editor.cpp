#include "engine/editor.h"

#include "engine/input_manager.h"
#include "engine/kryga_engine.h"
#include "engine/editor_system.h"

#include <global_state/global_state.h>

#include <native/native_window.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/render_system.h>

#include <render_translator/render_translator.h>
#include <render_translator/render_commands.h>

#include <imgui.h>
#include <ImGuizmo.h>

#include <core/level.h>
#include <core/level_manager.h>
#include <core/model_system.h>
#include <core/subsystem_queues.h>
#include <core/package_manager.h>

#include <packages/root/model/game_object.h>
#include <packages/base/model/mesh_object.h>
#include <packages/base/model/components/mesh_component.h>
#include <packages/base/model/components/camera_component.h>
#include <packages/base/model/components/input_component.h>
#include <packages/base/model/lights/point_light.h>
#include <packages/base/model/lights/directional_light.h>
#include <packages/base/model/lights/spot_light.h>
#include <packages/base/model/destructible_mesh_object.h>
#include <packages/base/model/components/destructible_mesh_component.h>
#include <packages/base/model/assets/destructible_mesh_asset.h>
#include <packages/tbs/model/hex_grid.h>
#include <packages/base/model/components/camera_component.h>

#include <gpu_types/gpu_generic_constants.h>

#include <packages/base/model/player.h>

#include <game/game_system_manager.h>

namespace kryga
{
namespace engine
{

void
game_editor::init()
{
    m_camera_data = {};
    m_position = {0.f, 0.f, 0.f};

    glob::glob_state().get_input_manager()->register_scaled_action(
        AID("move_forward"), this, &game_editor::ev_move_forward);
    glob::glob_state().get_input_manager()->register_scaled_action(
        AID("move_left"), this, &game_editor::ev_move_left);
    glob::glob_state().get_input_manager()->register_scaled_action(
        AID("look_up"), this, &game_editor::ev_look_up);
    glob::glob_state().get_input_manager()->register_scaled_action(
        AID("look_left"), this, &game_editor::ev_look_left);

    glob::glob_state().get_input_manager()->register_fixed_action(
        AID("level_reload"), true, this, &game_editor::ev_reload);

    glob::glob_state().get_input_manager()->register_fixed_action(
        AID("spawn"), true, this, &game_editor::ev_spawn);

    glob::glob_state().get_input_manager()->register_fixed_action(
        AID("lights"), true, this, &game_editor::ev_lights);

    glob::glob_state().get_input_manager()->register_fixed_action(
        AID("mouse_pressed"), true, this, &game_editor::ev_mouse_press);

    glob::glob_state().get_input_manager()->register_fixed_action(
        AID("shatter_demo"), true, this, &game_editor::ev_shatter_demo);

    glob::glob_state().get_input_manager()->register_fixed_action(
        AID("screenshot_select"),
        true,
        &glob::glob_state().getr_editor_system().screenshot,
        &screenshot_capture::toggle_selection);

    glob::glob_state().get_input_manager()->register_fixed_action(
        AID("toggle_play"), true, this, &game_editor::ev_toggle_play);

    glob::glob_state().get_input_manager()->register_fixed_action(
        AID("escape"), true, this, &game_editor::ev_escape);
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
    if (m_mode == editor_mode::playing)
    {
        return;
    }

    if (glob::glob_state().getr_editor_system().screenshot.is_selecting())
    {
        return;
    }

    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing())
    {
        return;
    }

    uint32_t w = glob::glob_state().getr_input_manager().get_mouse_state().x;
    uint32_t h = glob::glob_state().getr_input_manager().get_mouse_state().y;

    // Picking reads render-thread-owned state (the per-frame draw queues + the
    // picking BVH) and returns a render_data pointer — none of which may be
    // touched from this (main) thread. Defer the pick to the render thread, then
    // bounce the resolved id back to main for the model lookup + selection.
    // Fire-and-forget (NOT a blocking wait): mouse-press runs on the main thread,
    // which can't submit the frame that would drain a blocking render action.
    // The ~2-frame latency to a click is imperceptible.
    auto& renderer = glob::glob_state().getr_render().renderer;
    renderer.post_render_action(
        [this, w, h]()
        {
            auto* robj = glob::glob_state().getr_render().renderer.object_id_under_coordinate(w, h);
            utils::id picked = robj ? robj->id() : utils::id();
            glob::glob_state().getr_engine().queue_main_action([this, picked]()
                                                               { apply_pick(picked); });
        });
}

void
game_editor::apply_pick(const utils::id& clicked_id)
{
    if (!clicked_id.valid())
    {
        set_selected({});
        return;
    }

    auto* lvl = glob::glob_state().getr_model().current_level;
    if (!lvl)
    {
        return;
    }

    auto* comp = lvl->find_component(clicked_id);
    auto* go = comp ? comp->get_owner() : nullptr;
    auto new_sel = go ? go->get_id() : clicked_id;

    set_selected(new_sel == m_selected ? utils::id() : new_sel);
}

void
game_editor::ev_reload()
{
    if (m_mode == editor_mode::playing)
    {
        return;
    }

    auto& level = *glob::glob_state().getr_model().current_level;

    glob::glob_state().getr_model().drop_pending();
    glob::glob_state().getr_subsystem_queues().drop_pending();

    glob::glob_state().getr_render().renderer.clear_upload_queue();

    auto pids = level.get_package_ids();

    glob::glob_state().getr_engine().unload_render_resources(level);

    auto& lm = glob::glob_state().getr_model().levels;
    auto& pm = glob::glob_state().getr_model().packages;

    lm.unload_level(level);

    for (auto& id : pids)
    {
        auto p = pm.get_package(id);
        glob::glob_state().getr_engine().unload_render_resources(*p);
        pm.unload_package(*p);
    }

    glob::glob_state().getr_engine().init_scene();
}

void
game_editor::ev_spawn2()
{
    tbs::hex_grid::construct_params cprms;
    auto pp = glob::glob_state().getr_model().current_level->spawn_object<tbs::hex_grid>(AID("gg"),
                                                                                         cprms);
}

void
game_editor::ev_spawn()
{
    if (m_mode == editor_mode::playing)
    {
        return;
    }

#if 0

    if (glob::glob_state().getr_model().current_level->find_game_object(AID("obj_0_0_0")))
    {
        return;
    }

    core::spawn_parameters sp;

    int x = 0, y = 0, z = 0;

    int obj_DIM = 1;

    for (x = -obj_DIM / 2; x < obj_DIM / 2; ++x)
    {
        for (y = -obj_DIM / 2; y < obj_DIM / 2; ++y)
        {
            for (z = -obj_DIM / 2; z < obj_DIM / 2; ++z)
            {
                auto id = std::format("obj_{}_{}_{}", x, y, z);

                sp.position = root::vec3{x * 15.f, y * 15.f, z * 15.f};
                sp.scale = root::vec3{20, 2, 20};
                auto pp =
                    glob::glob_state()
                        .getr_model().current_level
                        ->spawn_object_as_clone<base::mesh_object>(AID("test_cube"), AID(id), sp);
                auto mc = pp->get_component_at(1)->as<base::mesh_component>();
                mc->layers().visible = true;
            }
        }
    }

    int light_DIM = 0;

    base::point_light::construct_params prms;

    for (x = -light_DIM / 2; x < light_DIM / 2; ++x)
    {
        for (y = -light_DIM / 2; y < light_DIM / 2; ++y)
        {
            for (z = -light_DIM / 2; z < light_DIM / 2; ++z)
            {
                auto id = std::format("pl_{}_{}_{}", x, y, z);

                prms.pos = root::vec3{x * 45.f, y * 45.f, z * 45.f};
                auto pp = glob::glob_state().getr_model().current_level->spawn_object<base::point_light>(
                    AID(id), prms);
            }
        }
    }
#endif
}

void
game_editor::ev_lights()
{
    if (m_mode == editor_mode::playing)
    {
        return;
    }

    auto& lvl = *glob::glob_state().getr_model().current_level;

    if (lvl.find_game_object(AID("PL1")))
    {
        return;
    }

    //     {
    //         base::spot_light::construct_params plp;
    //         plp.pos = {-20.f};
    //         lvl.spawn_object<base::spot_light>(AID("PL1"), plp);
    //     }
    //
    //     {
    //         base::point_light::construct_params plp;
    //         plp.pos = {15.f};
    //         lvl.spawn_object<base::point_light>(AID("PL2"), plp);
    //     }

    {
        base::directional_light::construct_params dcp;
        dcp.pos = {0.f, 20.f, 0.0};
        lvl.spawn_object<base::directional_light>(AID("DL"), dcp);
    }
}

void
game_editor::ev_shatter_demo()
{
    auto* lvl = glob::glob_state().getr_model().current_level;
    if (!lvl)
    {
        return;
    }

    auto& caches = glob::glob_state().getr_model().caches;
    auto* obj = caches.objects.get_item(AID("test_destructible"));
    auto* asset = obj ? obj->as<base::destructible_mesh_asset>() : nullptr;
    if (!asset)
    {
        ALOG_WARN("shatter_demo: test_destructible asset not found");
        return;
    }

    static int s_counter = 0;
    auto obj_id = AID(std::format("shatter_demo_{}", s_counter++));

    base::destructible_mesh_object::construct_params go_params;
    auto& cam = glob::glob_state().getr_render().renderer.get_camera();
    glm::vec3 cam_pos = cam.position;
    glm::vec3 cam_fwd = -glm::vec3(cam.view[0][2], cam.view[1][2], cam.view[2][2]);
    glm::vec3 spawn_pos = cam_pos + glm::normalize(cam_fwd) * 8.0f;
    go_params.pos = root::vec3{spawn_pos.x, spawn_pos.y, spawn_pos.z};

    auto* go = lvl->spawn_object<base::destructible_mesh_object>(obj_id, go_params);
    if (!go)
    {
        return;
    }

    go->get_root_component()->set_scale({3.0f, 3.0f, 3.0f});

    base::destructible_mesh_component::construct_params dmc_params;
    dmc_params.asset_handle = asset;

    auto* dmc = go->spawn_component<base::destructible_mesh_component>(
        go->get_root_component(), AID(obj_id.str() + "_dmc"), dmc_params);

    if (dmc && m_mode == editor_mode::playing)
    {
        m_pending_shatter = dmc;
        m_pending_shatter_frames = 0;
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
    if (m_pending_shatter && ++m_pending_shatter_frames > 2)
    {
        m_pending_shatter->shatter();
        m_pending_shatter = nullptr;
        m_pending_shatter_frames = 0;
    }

    if (m_mode == editor_mode::playing)
    {
    }
    else
    {
        update_camera();
    }
}

void
game_editor::update_camera()
{
    if (!m_updated)
    {
        return;
    }

    if (glob::glob_state().get_input_manager()->get_input_state(kryga::core::mouse_right))
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

    float speed = get_camera_speed_multiplier();
    m_velocity = (m_forward_delta * forward + m_left_delta * right) * speed;
    m_camera_data.position += m_velocity;

    m_forward_delta = 0.f;
    m_left_delta = 0.f;
    m_look_left_delta = 0.f;
    m_look_up_delta = 0.f;

    m_camera_data.view =
        glm::transpose(cam_rot) * glm::translate(glm::mat4{1.f}, -m_camera_data.position);

    float aspect = glob::glob_state().getr_native_window().aspect_ratio();
    if (aspect != m_cached_aspect_ratio)
    {
        m_cached_aspect_ratio = aspect;
        m_camera_data.projection =
            glm::perspective(glm::radians(60.f), aspect, (float)KGPU_znear, (float)KGPU_zfar);
        m_camera_data.projection[1][1] *= -1;
        m_camera_data.inv_projection = glm::inverse(m_camera_data.projection);
    }

    m_updated = false;
}

void
game_editor::set_camera(glm::vec3 position, float pitch, float yaw)
{
    m_camera_data.position = position;
    m_pitch = glm::clamp(pitch, -85.f, 85.f);
    m_yaw = yaw;
    m_updated = true;
}

gpu::camera_data
game_editor::get_camera_data()
{
    return m_camera_data;
}

editor_mode
game_editor::get_mode() const
{
    return m_mode;
}

void
game_editor::enter_play_mode()
{
    if (m_mode == editor_mode::playing)
    {
        return;
    }

    m_saved_position = m_camera_data.position;
    m_saved_pitch = m_pitch;
    m_saved_yaw = m_yaw;
    auto& dbg = glob::glob_state().getr_render().renderer.get_render_config().debug;
    m_saved_grid_visible = dbg.show_grid;
    m_saved_editor_mode_visuals = dbg.editor_mode;
    // Master gate — disables grid, gizmo billboards, debug wireframes, light
    // icons. The "preview as game" runtime switch.
    dbg.show_grid = false;
    dbg.editor_mode = false;

    m_active_camera = nullptr;
    m_player = nullptr;

    if (auto lvl = glob::glob_state().getr_model().current_level)
    {
        lvl->snapshot();

        base::player::construct_params pp;
        m_player = lvl->spawn_object<base::player>(AID("player_0"), pp);
        if (m_player)
        {
            m_player->set_position(root::vec3{
                m_camera_data.position.x, m_camera_data.position.y, m_camera_data.position.z});

            m_active_camera = m_player->get_camera();
            m_active_camera->set_active_camera(true);
            m_active_camera->set_perspective(60.f,
                                             glob::glob_state().getr_native_window().aspect_ratio(),
                                             (float)KGPU_znear,
                                             (float)KGPU_zfar);
        }
    }

    m_mode = editor_mode::playing;

    if (auto lvl2 = glob::glob_state().getr_model().current_level)
    {
        for (auto& [id, obj] : lvl2->get_game_objects().get_items())
        {
            if (auto go = obj->as<root::game_object>())
            {
                go->begin_play();
            }
        }
    }

    glob::glob_state().getr_game_system_manager().on_begin_play();
}

void
game_editor::exit_play_mode()
{
    if (m_mode == editor_mode::editor)
    {
        return;
    }

    glob::glob_state().getr_game_system_manager().on_end_play();

    if (auto lvl = glob::glob_state().getr_model().current_level)
    {
        for (auto& [id, obj] : lvl->get_game_objects().get_items())
        {
            if (auto go = obj->as<root::game_object>())
            {
                go->end_play();
            }
        }
    }

    if (auto lvl = glob::glob_state().getr_model().current_level)
    {
        lvl->rollback();
    }
    m_player = nullptr;
    m_active_camera = nullptr;

    m_camera_data.position = m_saved_position;
    m_pitch = m_saved_pitch;
    m_yaw = m_saved_yaw;
    m_updated = true;
    auto& dbg = glob::glob_state().getr_render().renderer.get_render_config().debug;
    dbg.show_grid = m_saved_grid_visible;
    dbg.editor_mode = m_saved_editor_mode_visuals;

    m_mode = editor_mode::editor;
}

void
game_editor::ev_toggle_play()
{
    if (m_mode == editor_mode::editor)
    {
        enter_play_mode();
    }
    else
    {
        exit_play_mode();
    }
}

void
game_editor::ev_escape()
{
    if (m_mode == editor_mode::playing)
    {
        exit_play_mode();
    }
}

base::camera_component*
game_editor::get_active_camera() const
{
    return m_active_camera;
}

void
game_editor::set_selected(const utils::id& id)
{
    auto* lvl = glob::glob_state().getr_model().current_level;

    // Toggle the outline flag via a render command, NOT by touching the render
    // cache/queues here: those are render-thread-owned and iterated during draw,
    // so a direct mutation from this (main) thread races the render thread under
    // the decoupled pipeline. The command resolves the id and re-buckets the
    // object on the render thread (see set_outline_cmd).
    auto set_outline = [&](const utils::id& sel_id, bool value)
    {
        if (!sel_id.valid() || !lvl)
        {
            return;
        }
        auto* obj = lvl->find_object(sel_id);
        if (!obj)
        {
            return;
        }

        auto& rb = glob::glob_state().getr_render_translator();
        auto enqueue_outline = [&](root::smart_object* comp)
        {
            // Components without a render object (lights, camera, ...) carry a null
            // handle; set_outline_cmd resolves it to null and no-ops.
            auto h = comp->get_render_object_handle();
            if (!h)
            {
                return;
            }
            auto* cmd = rb.alloc_cmd<set_outline_cmd>();
            cmd->obj_handle = h;
            cmd->outlined = value;
            rb.enqueue_cmd(cmd);
        };

        if (auto* go = obj->as<root::game_object>())
        {
            for (auto* comp : go->get_subcomponents())
            {
                enqueue_outline(comp);
            }
        }
        else
        {
            enqueue_outline(obj);
        }
    };

    set_outline(m_selected, false);
    m_selected = id;
    set_outline(m_selected, true);
}

utils::id
game_editor::get_selected() const
{
    return m_selected;
}

root::game_object*
game_editor::get_selected_game_object() const
{
    if (!m_selected.valid())
    {
        return nullptr;
    }

    auto* lvl = glob::glob_state().getr_model().current_level;
    if (!lvl)
    {
        return nullptr;
    }

    auto* obj = lvl->find_object(m_selected);
    if (!obj)
    {
        return nullptr;
    }

    return obj->as<root::game_object>();
}

}  // namespace engine

}  // namespace kryga
