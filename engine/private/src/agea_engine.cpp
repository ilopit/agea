#include "engine/agea_engine.h"

#include "engine/ui.h"
#include "engine/input_manager.h"
#include "engine/editor.h"
#include "engine/config.h"
#include "engine/engine_counters.h"

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
#include <core/global_state.h>
#include <core/core_state.h>

#include <global_state/global_state.h>

#include <native/native_window.h>

#include <packages/root/model/assets/mesh.h>
#include <packages/base/model/components/mesh_component.h>
#include <packages/root/model/game_object.h>
#include <packages/root/model/assets/shader_effect.h>

#include <packages/base/model/lights/directional_light.h>
#include <packages/base/model/lights/point_light.h>
#include <packages/base/model/lights/spot_light.h>
#include <packages/root/model/assets/material.h>

#include <packages/root/package.root.h>
#include <packages/base/package.base.h>

#include <render_bridge/render_bridge.h>

#include <resource_locator/resource_locator_state.h>

#include <vulkan_render/agea_render.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/vk_descriptors.h>

#include <utils/agea_log.h>
#include <utils/process.h>
#include <utils/clock.h>

#include <sol2_unofficial/sol.h>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <thread>

namespace agea
{
glob::engine::type glob::engine::type::s_instance;

vulkan_engine::vulkan_engine(std::unique_ptr<singleton_registry> r)
    : m_registry(std::move(r))
    , m_sync_service(std::make_unique<sync_service>())
{
}

vulkan_engine::vulkan_engine()
    : m_sync_service()
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
vulkan_engine::init()
{
    ALOG_INFO("Initialization started ...");

    auto& gs = glob::glob_state();

    gs.schedule_action(gs::state::state_stage::create,
                       [](gs::state& s)
                       {
                           // state
                           state_mutator__resource_locator::set(s);

                           core::state_mutator__caches::set(s);
                           core::state_mutator__level_manager::set(s);
                           core::state_mutator__package_manager::set(s);
                           core::state_mutator__reflection_manager::set(s);
                           core::state_mutator__lua_api::set(s);
                           core::state_mutator__id_generator::set(s);
                       });

    gs.run_create();

    glob::game_editor::create(*m_registry);
    glob::input_manager::create(*m_registry);
    glob::config::create(*m_registry);
    glob::render_device::create(*m_registry);
    glob::vulkan_render_loader::create(*m_registry);
    glob::ui::create(*m_registry);
    glob::native_window::create(*m_registry);
    glob::vulkan_render::create(*m_registry);
    glob::engine_counters::create(*m_registry);
    glob::render_bridge::create(*m_registry);

    gs.run_connect();
    init_default_scripting();

    glob::glob_state().get_resource_locator()->init_local_dirs();
    auto cfgs_folder = glob::glob_state().get_resource_locator()->resource_dir(category::configs);

    utils::path main_config = cfgs_folder / "agea.acfg";
    glob::config::get()->load(main_config);

    utils::path input_config = cfgs_folder / "inputs.acfg";
    glob::input_manager::get()->load_actions(input_config);

    glob::game_editor::get()->init();

    gs.schedule_action(gs::state::state_stage::init,
                       [](agea::gs::state& s) { s.get_pm()->init(); });
    gs.run_init();

    native_window::construct_params rwc;
    rwc.w = 1600 * 2;
    rwc.h = 900 * 2;
    auto window = glob::native_window::get();

    if (!window->construct(rwc))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    render::render_device::construct_params rdc;
    rdc.window = window->handle();

    auto device = glob::render_device::get();
    if (!device->construct(rdc))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    glob::ui::getr().init();

    glob::vulkan_render::getr().init(rwc.w, rwc.h);

    init_default_resources();

    init_scene();

    m_sync_service->start();

    ALOG_INFO("Initialization completed");
    return true;
}
void
vulkan_engine::cleanup()
{
    glob::render_device::get()->wait_for_fences();

    glob::vulkan_render::getr().deinit();

    glob::vulkan_render_loader::get()->clear_caches();

    glob::render_device::get()->destruct();
}

void
vulkan_engine::run()
{
    float frame_time = 1.f / glob::config::get()->fps_lock;
    const std::chrono::microseconds frame_time_int(1000000 / glob::config::get()->fps_lock);

    // main loop
    for (;;)
    {
        AGEA_make_scope(frame);

        auto start_ts = utils::get_current_time_mks();

        {
            AGEA_make_scope(input);

            if (!glob::input_manager::get()->input_tick(frame_time))
            {
                break;
            }

            glob::input_manager::get()->fire_input_event();
        }
        {
            AGEA_make_scope(ui_tick);
            glob::ui::get()->new_frame(frame_time);
        }
        {
            AGEA_make_scope(tick);
            tick(frame_time);
        }
        {
            AGEA_make_scope(sync);
            execute_sync_requests();
        }
        {
            AGEA_make_scope(consume_updates);

            update_cameras();
            glob::vulkan_render::getr().set_camera(m_camera_data);

            if (glob::glob_state().get_current_level())
            {
                consume_updated_shader_effects();
                consume_updated_render_assets();
                consume_updated_render_components();
                consume_updated_transforms();
            }
        }
        {
            AGEA_make_scope(draw);

            glob::vulkan_render::getr().draw_main();
        }

        auto frame_msk = std::chrono::microseconds(utils::get_current_time_mks() - start_ts);

        if (frame_msk < frame_time_int)
        {
            stupid_sleep(std::chrono::microseconds(frame_time_int - frame_msk));
        }

        frame_msk = std::chrono::microseconds(utils::get_current_time_mks() - start_ts);
        frame_time = 0.00001f * frame_msk.count();
    }
}
void
vulkan_engine::tick(float dt)
{
    glob::game_editor::get()->on_tick(dt);
    if (auto lvl = glob::glob_state().get_current_level())
    {
        lvl->tick(dt);
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

            auto dep = glob::render_bridge::getr().get_dependency().get_node(ptr);

            glob::render_bridge::getr().get_dependency().print(false);

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
        ALOG_FATAL("Nothign to do here!");
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
        glob::render_bridge::getr().render_dtor(*t.second, true);
    }

    return true;
}

bool
vulkan_engine::unload_render_resources(core::package& l)
{
    auto& cs = l.get_local_cache();

    for (auto& t : cs.objects.get_items())
    {
        glob::render_bridge::getr().render_dtor(*t.second, true);
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
                    obj_data->gpu_data.model_matrix = m->get_transofrm_matrix();

                    glob::vulkan_render::getr().schedule_game_data_gpu_upload(obj_data);
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
    m_camera_data = glob::game_editor::get()->get_camera_data();
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

    auto vertices = vert_buffer.make_view<render::gpu_vertex_data>();
    auto indices = index_buffer.make_view<render::gpu_index_data>();

    glob::vulkan_render_loader::getr().create_mesh(AID("plane_mesh"), vertices, indices);

    // auto pkg = glob::package_manager::getr().get_package(AID("root"));
    //
    //     root::mesh::construct_params p;
    //     p.indices = index_buffer;
    //     p.vertices = vert_buffer;
    //
    //     auto obj = pkg->add_object<root::mesh>(AID("plane_mesh"), p);
    //
    //     glob::render_bridge::getr().render_ctor(*obj, true);
}

void
vulkan_engine::init_scene()
{
    auto level_id = AID("light_sandbox");
    if (level_id.valid())
    {
        load_level(level_id);
        //
        // glob::game_editor::getr().ev_spawn();
        // glob::game_editor::getr().ev_lights();
    }
}

void
vulkan_engine::init_default_scripting()
{
    auto lua = glob::glob_state().get_lua();

    static auto rt = lua->state().new_usertype<utils::id>("reflection_type", sol::no_constructor);

    static auto aid = lua->state().new_usertype<reflection::reflection_type>(
        "aid", sol::no_constructor, "i", [](const char* id) -> utils::id { return AID(id); });

    static auto p = lua->state().new_usertype<core::package>("package", sol::no_constructor);

    auto pm = glob::glob_state().get_pm();

    static auto lua_pm = lua->state().new_usertype<core::package_manager>(
        "pm", sol::no_constructor, "get_package",
        [pm](const char* id) -> core::package* { return pm->get_package(AID(id)); }, "load",
        [pm](const char* id) -> bool { return pm->load_package(AID(id)); });
}

void
vulkan_engine::consume_updated_render_components()
{
    auto& items = glob::glob_state().get_current_level()->get_dirty_render_queue();

    for (auto& i : items)
    {
        glob::render_bridge::getr().render_ctor(*i, true);
    }

    items.clear();
}

void
vulkan_engine::consume_updated_render_assets()
{
    auto& items = glob::glob_state().get_current_level()->get_dirty_render_assets_queue();

    for (auto& i : items)
    {
        glob::render_bridge::getr().render_ctor(*i, true);
    }

    items.clear();
}

void
vulkan_engine::consume_updated_shader_effects()
{
    auto& items = glob::glob_state().get_current_level()->get_dirty_shader_effect_queue();

    for (auto& i : items)
    {
        glob::render_bridge::getr().render_ctor(*i, true);
    }

    items.clear();
}

}  // namespace agea
