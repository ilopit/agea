#include "engine/kryga_engine.h"

#include "engine/ui.h"
#include "engine/input_manager.h"
#include "engine/editor.h"
#include "engine/config.h"
#include "engine/engine_counters.h"
#include "engine/profiler.h"

#include "engine/private/sync_service.h"

#include <core/caches/caches_map.h>
#include <core/id_generator.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <core/object_constructor.h>
#include <core/package.h>
#include <core/package_manager.h>
#include <core/reflection/lua_api.h>
#include <core/reflection/reflection_type.h>
#include <core/core_state.h>

#include <global_state/global_state.h>

#include <native/native_window.h>

#include <packages/root/model/assets/mesh.h>
#include <packages/base/model/components/mesh_component.h>
#include <packages/root/model/game_object.h>
#include <packages/root/model/assets/shader_effect.h>
#include <packages/base/model/camera_object.h>

#include <packages/base/model/lights/directional_light.h>
#include <packages/base/model/lights/point_light.h>
#include <packages/base/model/lights/spot_light.h>
#include <packages/base/model/components/camera_component.h>
#include <packages/base/model/components/input_component.h>
#include <packages/root/model/assets/material.h>

#include <packages/root/package.root.h>
#include <packages/base/package.base.h>

#include <render_bridge/render_bridge.h>

#include <resource_locator/resource_locator_state.h>

#include <vulkan_render/kryga_render.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/types/vulkan_mesh_data.h>
#include <vulkan_render/vk_descriptors.h>

#include <animation/animation_system.h>

#include <utils/kryga_log.h>
#include <utils/process.h>
#include <utils/clock.h>

#include <sol2_unofficial/sol.h>

#include <fstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>

namespace kryga
{

// ============================================================================
// Startup Options
// ============================================================================

bool
startup_options::parse(int argc, char** argv, startup_options& out)
{
    out = {};  // Reset to defaults

    for (int i = 1; i < argc; ++i)
    {
        const char* arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
        {
            out.show_help = true;
            return false;
        }
        else if (strcmp(arg, "--render-mode") == 0 || strcmp(arg, "-r") == 0)
        {
            if (i + 1 >= argc)
            {
                ALOG_ERROR("{} requires a value (instanced|per_object)", arg);
                return false;
            }
            ++i;
            const char* value = argv[i];

            if (strcmp(value, "instanced") == 0)
            {
                out.render_mode = render::render_mode::instanced;
            }
            else if (strcmp(value, "per_object") == 0)
            {
                out.render_mode = render::render_mode::per_object;
            }
            else
            {
                ALOG_ERROR("Unknown render mode '{}'. Valid values: instanced, per_object", value);
                return false;
            }
        }
        else if (strcmp(arg, "--run-for") == 0 || strcmp(arg, "-t") == 0)
        {
            if (i + 1 >= argc)
            {
                ALOG_ERROR("{} requires a value (seconds)", arg);
                return false;
            }
            ++i;
            const char* value = argv[i];

            char* end = nullptr;
            float seconds = std::strtof(value, &end);
            if (end == value || seconds < 0.f)
            {
                ALOG_ERROR("Invalid run duration '{}'. Must be a non-negative number.", value);
                return false;
            }
            out.run_for_seconds = seconds;
        }
        else
        {
            ALOG_ERROR("Unknown argument '{}'", arg);
            out.show_help = true;
            return false;
        }
    }

    return true;
}

void
startup_options::print_help(const char* program_name)
{
    ALOG_INFO(
        "Usage: {} [OPTIONS]\n\n"
        "Options:\n"
        "  -h, --help              Show this help message\n"
        "  -r, --render-mode MODE  Set render mode (default: instanced)\n"
        "                          Values: instanced, per_object\n"
        "  -t, --run-for SECONDS   Run for specified duration then exit\n"
        "                          (0 or omit for unlimited)\n\n"
        "Render Modes:\n"
        "  instanced   - Batched instanced drawing with GPU cluster culling\n"
        "  per_object  - Per-object drawing with CPU light grid",
        program_name);
}

vulkan_engine::vulkan_engine()
    : m_sync_service(std::make_unique<sync_service>())
{
}

void
stupid_sleep(std::chrono::microseconds sleep_for)
{
    auto current = std::chrono::high_resolution_clock::now();
    auto to = current + sleep_for;
    auto dt = to - current;

    while (dt > std::chrono::microseconds(500) && current < to)
    {
        dt /= 2;

        std::this_thread::sleep_for(dt);
        current = std::chrono::high_resolution_clock::now();
        dt = to - current;
    }
}

vulkan_engine::~vulkan_engine()
{
}

bool
vulkan_engine::init(const startup_options& options)
{
    ALOG_INFO("Initialization started ...");
    ALOG_INFO("Render mode: {}",
              options.render_mode == render::render_mode::instanced ? "INSTANCED" : "PER_OBJECT");

    m_run_for_seconds = options.run_for_seconds;
    if (m_run_for_seconds > 0.f)
    {
        ALOG_INFO("Run duration limit: {} seconds", m_run_for_seconds);
    }

    auto& gs = glob::glob_state();
    core::state_mutator__lua_api::set(gs);
    core::state_mutator__caches::set(gs);
    core::state_mutator__reflection_manager::set(gs);
    core::state_mutator__package_manager::set(gs);

    gs.schedule_action(gs::state::state_stage::create,
                       [](gs::state& s)
                       {
                           state_mutator__resource_locator::set(s);
                           core::state_mutator__level_manager::set(s);
                           core::state_mutator__id_generator::set(s);
                       });

    gs.run_create();

    state_mutator__config::set(gs);
    state_mutator__game_editor::set(gs);
    state_mutator__input_manager::set(gs);
    glob::set_input_provider(glob::glob_state().get_input_manager());
    state_mutator__render_device::set(gs);
    state_mutator__vulkan_render::set(gs);
    state_mutator__vulkan_render_loader::set(gs);
    state_mutator__ui::set(gs);
    state_mutator__native_window::set(gs);
    state_mutator__engine_counters::set(gs);
    state_mutator__render_bridge::set(gs);
    state_mutator__animation_system::set(gs);

    gs.run_connect();
    init_default_scripting();

    glob::glob_state().get_resource_locator()->init_local_dirs();
    auto cfgs_folder = glob::glob_state().get_resource_locator()->resource_dir(category::configs);

    utils::path main_config = cfgs_folder / "kryga.acfg";
    glob::glob_state().get_config()->load(main_config);

    utils::path input_config = cfgs_folder / "inputs.acfg";
    glob::glob_state().get_input_manager()->load_actions(input_config);

    glob::glob_state().get_game_editor()->init();

    gs.schedule_action(gs::state::state_stage::init,
                       [](kryga::gs::state& s) { s.get_pm()->init(); });
    gs.run_init();

    native_window::construct_params rwc;
    rwc.w = 1600 * 2;
    rwc.h = 900 * 2;
    auto window = glob::glob_state().get_native_window();

    if (!window->construct(rwc))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    render::render_device::construct_params rdc;
    rdc.window = window->handle();

    auto device = glob::glob_state().get_render_device();
    if (!device->construct(rdc))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    glob::glob_state().getr_ui().init();

    glob::glob_state().getr_vulkan_render().init(rwc.w, rwc.h, options.render_mode);

    init_default_resources();

    init_scene();

    m_sync_service->start();

    ALOG_INFO("Initialization completed");
    return true;
}

void
vulkan_engine::cleanup()
{
    glob::set_input_provider(nullptr);

    m_sync_service->stop();

    glob::glob_state().get_render_device()->wait_for_fences();

    glob::glob_state().getr_vulkan_render().deinit();

    glob::glob_state().get_vulkan_render_loader()->clear_caches();

    glob::glob_state().get_render_device()->destruct();

    glob::glob_state_reset();
}

void
vulkan_engine::run()
{
    float frame_time = 1.f / glob::glob_state().get_config()->fps_lock;
    const std::chrono::microseconds frame_time_int(1000000 /
                                                   glob::glob_state().get_config()->fps_lock);

    float total_elapsed = 0.f;

    // main loop
    for (;;)
    {
        // Check run duration limit
        if (m_run_for_seconds > 0.f && total_elapsed >= m_run_for_seconds)
        {
            ALOG_INFO("Run duration limit reached ({} seconds), exiting.", m_run_for_seconds);
            break;
        }

        KRG_make_scope(frame);
        KRG_PROFILE_SCOPE("Frame");

        auto start_ts = utils::get_current_time_mks();

        {
            KRG_make_scope(input);
            KRG_PROFILE_SCOPE("Input");

            if (!glob::glob_state().get_input_manager()->input_tick(frame_time))
            {
                break;
            }

            glob::glob_state().get_input_manager()->fire_input_event();
        }
        {
            KRG_make_scope(ui_tick);
            KRG_PROFILE_SCOPE("UI");
            glob::glob_state().get_ui()->new_frame(frame_time);
        }
        {
            KRG_make_scope(tick);
            KRG_PROFILE_SCOPE("Tick");
            tick(frame_time);
        }
        {
            KRG_make_scope(sync);
            KRG_PROFILE_SCOPE("Sync");
            execute_sync_requests();
        }
        {
            KRG_make_scope(consume_updates);
            KRG_PROFILE_SCOPE("ConsumeUpdates");

            update_cameras();
            glob::glob_state().getr_vulkan_render().set_camera(m_camera_data);

            if (glob::glob_state().get_current_level())
            {
                consume_updated_shader_effects();
                consume_updated_render_assets();
                consume_updated_render_components();
                consume_updated_transforms();
            }
        }
        {
            KRG_make_scope(draw);
            KRG_PROFILE_SCOPE("Draw");

            glob::glob_state().getr_vulkan_render().draw_main();

            auto& ctrs = ::kryga::glob::glob_state().getr_engine_counters();
            auto& vr = glob::glob_state().getr_vulkan_render();

            ctrs.all_draws.update(vr.get_all_draws());
            ctrs.culled_draws.update(vr.get_culled_draws());
            ctrs.objects.update(vr.get_cache().objects.get_actual_size());
        }

        auto frame_msk = std::chrono::microseconds(utils::get_current_time_mks() - start_ts);

        if (frame_msk < frame_time_int)
        {
            stupid_sleep(std::chrono::microseconds(frame_time_int - frame_msk));
        }

        frame_msk = std::chrono::microseconds(utils::get_current_time_mks() - start_ts);
        frame_time = 0.00001f * frame_msk.count();
        total_elapsed += frame_time;

        KRG_PROFILE_FRAME_MARK();
    }
}
void
vulkan_engine::tick(float dt)
{
    glob::glob_state().get_game_editor()->on_tick(dt);
    if (glob::glob_state().get_game_editor()->get_mode() == engine::editor_mode::playing)
    {
        if (auto lvl = glob::glob_state().get_current_level())
        {
            lvl->tick(dt);
        }
    }

    if (auto* anim = glob::glob_state().get_animation_system())
    {
        anim->tick(dt);
    }
}

void
vulkan_engine::execute_sync_requests()
{
    if (!m_sync_service->has_sync_actions())
    {
        return;
    }

    std::vector<sync_action> actions;
    m_sync_service->extract_data(actions);

    for (auto& sa : actions)
    {
        std::string name, ext;
        sa.path_to_resources.parse_file_name_and_ext(name, ext);

        if (ext == "lua")
        {
            auto lua = glob::glob_state().get_lua();

            auto lua_r = lua->state().script_file(sa.path_to_resources.str());

            std::string result = lua->buffer();
            lua->reset();
            if (lua_r.status() != sol::call_status::ok)
            {
                sol::error err = lua_r;
                result += err.what();
            }

            sa.to_signal.set_value(result);
        }
        else if (ext == "vert" || ext == "frag")
        {
            auto sec = glob::glob_state().get_class_shader_effects_cache();

            auto ptr = sec->get_item(AID(name));
            ptr->mark_render_dirty();

            auto dep = glob::glob_state().getr_render_bridge().get_dependency().get_node(ptr);

            glob::glob_state().getr_render_bridge().get_dependency().print(false);

            for (auto o : dep.m_children)
            {
                auto mt = o->as<root::asset>();
                mt->mark_render_dirty();
            }

            sa.to_signal.set_value("");
        }
        else
        {
            sa.to_signal.set_value("");
        }
    }
}

bool
vulkan_engine::load_level(const utils::id& level_id)
{
    auto lm = glob::glob_state().get_lm();
    auto cs = glob::glob_state().get_class_set();
    auto is = glob::glob_state().get_instance_set();

    auto result = lm->load_level(level_id);
    if (!result)
    {
        ALOG_FATAL("Nothing to do here!");
        return false;
    }

    core::state_mutator__current_level::set(*result, glob::glob_state());

    return true;
}

bool
vulkan_engine::unload_render_resources(core::level& l)
{
    auto& cs = l.get_local_cache();

    for (auto& t : cs.objects.get_items())
    {
        glob::glob_state().getr_render_bridge().render_dtor(*t.second, true);
    }

    return true;
}

bool
vulkan_engine::unload_render_resources(core::package& l)
{
    auto& cs = l.get_local_cache();

    for (auto& t : cs.objects.get_items())
    {
        glob::glob_state().getr_render_bridge().render_dtor(*t.second, true);
    }

    return true;
}

void
vulkan_engine::consume_updated_transforms()
{
    auto& items = glob::glob_state().get_current_level()->get_dirty_transforms_components_queue();

    if (items.empty())
    {
        return;
    }

    for (auto& i : items)
    {
        auto r = i->get_owner()->get_components(i->get_order_idx());

        for (auto& obj : r)
        {
            if (auto m = obj.as<base::mesh_component>())
            {
                auto obj_data = m->get_render_object_data();
                if (obj_data)
                {
                    obj_data->gpu_data.model = m->get_transform_matrix();
                    obj_data->gpu_data.normal = m->get_normal_matrix();
                    obj_data->gpu_data.obj_pos = glm::vec3(m->get_world_position());

                    auto scale = m->get_scale();
                    float max_s =
                        glm::max(glm::max(glm::abs(scale.x), glm::abs(scale.y)), glm::abs(scale.z));
                    obj_data->gpu_data.bounding_radius = obj_data->mesh->m_bounding_radius * max_s;

                    glob::glob_state().getr_vulkan_render().schedule_game_data_gpu_upload(obj_data);
                }
                m->set_dirty_transform(false);
            }
        }

        i->set_dirty_transform(false);
    }

    items.clear();
}

void
vulkan_engine::update_cameras()
{
    auto editor = glob::glob_state().get_game_editor();
    auto* cam = editor->get_active_camera();
    if (editor->get_mode() == engine::editor_mode::playing && cam)
    {
        float aspect = glob::glob_state().getr_native_window().aspect_ratio();
        cam->set_aspect_ratio(aspect);

        m_camera_data.projection = cam->get_perspective();
        m_camera_data.inv_projection = cam->get_inv_projection();
        m_camera_data.view = cam->get_view();
        m_camera_data.position = cam->get_owner()->get_position();
    }
    else
    {
        m_camera_data = editor->get_camera_data();
    }
}

void
vulkan_engine::init_default_resources()
{
    auto vl = render::gpu_dynobj_builder()
                  .add_field(AID("vPosition"), render::gpu_type::g_vec3, 1)
                  .add_field(AID("vNormal"), render::gpu_type::g_vec3, 1)
                  .add_field(AID("vColor"), render::gpu_type::g_vec3, 1)
                  .add_field(AID("vTexCoord"), render::gpu_type::g_vec2, 1)
                  .finalize();

    auto val = render::gpu_dynobj_builder().add_array(AID("verts"), vl, 1, 4, 4).finalize();

    utils::buffer vert_buffer(val->get_object_size());

    {
        auto v = val->make_view<render::gpu_type>(vert_buffer.data());

        using v3 = glm::vec3;
        using v2 = glm::vec2;

        v.subobj(0, 0).write(v3{-1.f, 1.f, 0.f}, v3{0.f}, v3{0.f}, v2{0.f, 0.f});
        v.subobj(0, 1).write(v3{1.f, 1.f, 0.f}, v3{0.f}, v3{0.f}, v2{1.0, 0.f});
        v.subobj(0, 2).write(v3{-1.f, -1.f, 0.f}, v3{0.f}, v3{0.f}, v2{0.f, 1.f});
        v.subobj(0, 3).write(v3{1.f, -1.f, 0.f}, v3{0.f}, v3{0.f}, v2{1.f, 1.f});
    }

    utils::buffer index_buffer(6 * 4);
    auto v = index_buffer.make_view<uint32_t>();
    v.at(0) = 0;
    v.at(1) = 2;
    v.at(2) = 1;
    v.at(3) = 2;
    v.at(4) = 3;
    v.at(5) = 1;

    auto vertices = vert_buffer.make_view<gpu::vertex_data>();
    auto indices = index_buffer.make_view<gpu::uint>();

    glob::glob_state().getr_vulkan_render_loader().create_mesh(AID("plane_mesh"), vertices,
                                                               indices);

    // auto pkg = glob::package_manager::getr().get_package(AID("root"));
    //
    //     root::mesh::construct_params p;
    //     p.indices = index_buffer;
    //     p.vertices = vert_buffer;
    //
    //     auto obj = pkg->add_object<root::mesh>(AID("plane_mesh"), p);
    //
    //     glob::glob_state().getr_render_bridge().render_ctor(*obj, true);
}

void
vulkan_engine::init_scene()
{
    auto level_id = AID("light_sandbox");
    if (level_id.valid())
    {
        load_level(level_id);
        //
        glob::glob_state().getr_game_editor().ev_spawn();
        glob::glob_state().getr_game_editor().ev_lights();
    }

    if (auto lvl = glob::glob_state().get_current_level())
    {
        base::camera_object::construct_params co_prms;
        auto cam_obj = lvl->spawn_object<base::camera_object>(AID("play_camera"), co_prms);
        if (cam_obj)
        {
            cam_obj->get_camera_component()->set_active_camera(true);
            cam_obj->get_camera_component()->set_perspective(
                60.f, glob::glob_state().getr_native_window().aspect_ratio(), (float)KGPU_znear,
                (float)KGPU_zfar);
        }
    }
}

void
vulkan_engine::init_default_scripting()
{
    auto& gs = glob::glob_state();

    auto lua = gs.get_lua();

    auto rt = lua->state().new_usertype<utils::id>("reflection_type", sol::no_constructor);
    gs.create_box_with_obj("rt", std::move(rt));

    auto aid = lua->state().new_usertype<reflection::reflection_type>(
        "aid", sol::no_constructor, "i", [](const char* id) -> utils::id { return AID(id); });
    gs.create_box_with_obj("aid", std::move(aid));

    auto package = lua->state().new_usertype<core::package>("package", sol::no_constructor);
    gs.create_box_with_obj("package", std::move(package));

    auto pm = glob::glob_state().get_pm();
    auto lua_pm = lua->state().new_usertype<core::package_manager>(
        "pm", sol::no_constructor, "get_package",
        [pm](const char* id) -> core::package* { return pm->get_package(AID(id)); }, "load",
        [pm](const char* id) -> bool { return pm->load_package(AID(id)); });

    gs.create_box_with_obj("lua_pm", std::move(lua_pm));
}

void
vulkan_engine::consume_updated_render_components()
{
    auto& items = glob::glob_state().get_current_level()->get_dirty_render_queue();

    for (auto& i : items)
    {
        glob::glob_state().getr_render_bridge().render_ctor(*i, true);
    }

    items.clear();
}

void
vulkan_engine::consume_updated_render_assets()
{
    auto& items = glob::glob_state().get_current_level()->get_dirty_render_assets_queue();

    for (auto& i : items)
    {
        glob::glob_state().getr_render_bridge().render_ctor(*i, true);
    }

    items.clear();
}

void
vulkan_engine::consume_updated_shader_effects()
{
    auto& items = glob::glob_state().get_current_level()->get_dirty_shader_effect_queue();

    for (auto& i : items)
    {
        glob::glob_state().getr_render_bridge().render_ctor(*i, true);
    }

    items.clear();
}

}  // namespace kryga
