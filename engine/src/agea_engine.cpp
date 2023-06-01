﻿#include "engine/agea_engine.h"

#include "engine/ui.h"
#include "engine/input_manager.h"
#include "engine/editor.h"
#include "engine/config.h"
#include "engine/engine_counters.h"
#include "engine/active_modules.h"

#include <core/caches/caches_map.h>
#include <core/caches/components_cache.h>
#include <core/caches/materials_cache.h>
#include <core/caches/meshes_cache.h>
#include <core/caches/objects_cache.h>
#include <core/caches/textures_cache.h>
#include <core/caches/game_objects_cache.h>

#include <core/id_generator.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <core/object_constructor.h>
#include <core/package.h>
#include <core/package_manager.h>
#include <core/reflection/lua_api.h>

#include <native/native_window.h>

#include <root/assets/mesh.h>
#include <root/components/mesh_component.h>
#include <root/game_object.h>
#include <root/assets/shader_effect.h>

#include <root/lights/directional_light.h>
#include <root/lights/point_light.h>
#include <root/lights/spot_light.h>
#include <root/assets/material.h>

#include <render_bridge/render_bridge.h>

#include <vulkan_render/vulkan_render.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/vk_descriptors.h>

#include <utils/agea_log.h>
#include <utils/process.h>
#include <utils/clock.h>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <thread>

#if defined(AGEA_demo_module_included)

#include <demo/example.h>

#endif

namespace agea
{
glob::engine::type glob::engine::type::s_instance;

vulkan_engine::vulkan_engine(std::unique_ptr<singleton_registry> r)
    : m_registry(std::move(r))
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

    glob::game_editor::create(*m_registry);
    glob::input_manager::create(*m_registry);
    glob::resource_locator::create(*m_registry);
    glob::config::create(*m_registry);
    glob::render_device::create(*m_registry);
    glob::vulkan_render_loader::create(*m_registry);
    glob::ui::create(*m_registry);
    glob::level_manager::create(*m_registry);
    glob::native_window::create(*m_registry);
    glob::package_manager::create(*m_registry);
    glob::lua_api::create(*m_registry);
    glob::vulkan_render::create(*m_registry);
    glob::id_generator::create(*m_registry);
    glob::engine_counters::create(*m_registry);
    glob::module_manager::create(*m_registry);
    glob::reflection_type_registry::create(*m_registry);
    glob::render_bridge::create(*m_registry);

    glob::init_global_caches(*m_registry);

    engine::register_modules();

    for (auto m : glob::module_manager::getr().modules())
    {
        m->init_reflection();
        m->override_reflection_types();
    }

    glob::reflection_type_registry::getr().finilaze();

    glob::resource_locator::get()->init_local_dirs();
    auto cfgs_folder = glob::resource_locator::get()->resource_dir(category::configs);

    utils::path main_config = cfgs_folder / "agea.acfg";
    glob::config::get()->load(main_config);

    utils::path input_config = cfgs_folder / "inputs.acfg";
    glob::input_manager::get()->load_actions(input_config);

    glob::package_manager::getr().init();

    glob::game_editor::get()->init();

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

    glob::vulkan_render::getr().init();

    init_default_resources();

    init_scene();

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
            AGEA_make_scope(consume_updates);

            update_cameras();
            glob::vulkan_render::getr().set_camera(m_camera_data);

            consume_updated_shader_effects();
            consume_updated_render_assets();
            consume_updated_render_components();
            consume_updated_transforms();
        }
        {
            AGEA_make_scope(draw);

            glob::vulkan_render::getr().draw_objects();
        }

        glob::vulkan_render_loader::getr().delete_sheduled_actions();

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
    glob::level::get()->tick(dt);
}

bool
vulkan_engine::load_level(const utils::id& level_id)
{
    auto result = glob::level_manager::getr().load_level(glob::config::get()->level,
                                                         glob::proto_objects_cache_set::get(),
                                                         glob::objects_cache_set::get());
    if (!result)
    {
        ALOG_FATAL("Nothign to do here!");
        return false;
    }

    glob::level::create_ref(result);

    return true;
}

bool
vulkan_engine::unload_render_resources(core::level& l)
{
    auto& cs = l.get_local_cache();

    for (auto& t : cs.objects->get_items())
    {
        glob::render_bridge::getr().render_dtor(*t.second, true);
    }

    return true;
}

bool
vulkan_engine::unload_render_resources(core::package& l)
{
    auto& cs = l.get_local_cache();

    for (auto& t : cs.objects->get_items())
    {
        glob::render_bridge::getr().render_dtor(*t.second, true);
    }

    return true;
}

void
vulkan_engine::consume_updated_transforms()
{
    auto& items = glob::level::getr().get_dirty_transforms_components_queue();

    if (items.empty())
    {
        return;
    }

    for (auto& i : items)
    {
        auto r = i->get_owner()->get_components(i->get_order_idx());

        for (auto& obj : r)
        {
            if (auto m = obj.as<root::mesh_component>())
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
    utils::buffer vert_buffer;
    {
        render::gpu_dynobj_builder builder;
        builder.add_field(AID("vPosition"), render::gpu_type::g_vec3, 1);
        builder.add_field(AID("vNormal"), render::gpu_type::g_vec3, 1);
        builder.add_field(AID("vColor"), render::gpu_type::g_vec3, 1);
        builder.add_field(AID("vTexCoord"), render::gpu_type::g_vec2, 1);

        auto vert_obj = builder.make_obj(&vert_buffer.full_data());

        using v3 = glm::vec3;
        using v2 = glm::vec2;

        vert_obj.write_obj<render::gpu_type>(0, v3{-1.f, 1.f, 0.f}, v3{0.f}, v3{0.f}, v2{0.f, 0.f});
        vert_obj.write_obj<render::gpu_type>(0, v3{1.f, 1.f, 0.f}, v3{0.f}, v3{0.f}, v2{1.0, 0.f});
        vert_obj.write_obj<render::gpu_type>(0, v3{-1.f, -1.f, 0.f}, v3{0.f}, v3{0.f},
                                             v2{0.f, 1.f});
        vert_obj.write_obj<render::gpu_type>(0, v3{1.f, -1.f, 0.f}, v3{0.f}, v3{0.f}, v2{1.f, 1.f});
    }

    utils::buffer index_buffer(6 * 4);
    auto v = index_buffer.make_view<uint32_t>();
    v.at(0) = 0;
    v.at(1) = 2;
    v.at(2) = 1;
    v.at(3) = 2;
    v.at(4) = 3;
    v.at(5) = 1;

    auto pkg = glob::package_manager::getr().get_package(AID("root"));

    root::mesh::construct_params p;
    p.indices = index_buffer;
    p.vertices = vert_buffer;

    auto obj = pkg->add_object<root::mesh>(AID("plane_mesh"), p);

    glob::render_bridge::getr().render_ctor(*obj, true);
}

void
vulkan_engine::init_scene()
{
    load_level(glob::config::get()->level);

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

#if defined(AGEA_demo_module_included)

    demo::example::construct_params dcp;

    auto p = glob::level::getr().spawn_object<demo::example>(AID("spawned_example"), dcp);

#endif

    core::spawn_parameters sp;
    auto id1 = AID("decor");
    auto id2 = AID("decor");

    int x = 0, y = 0, z = 0;

    int DIM = 10;

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
                ALOG_INFO("Spawned {0}", p->get_id().cstr());
            }
        }
    }
}

void
vulkan_engine::consume_updated_render_components()
{
    auto& items = glob::level::getr().get_dirty_render_queue();

    for (auto& i : items)
    {
        glob::render_bridge::getr().render_ctor(*i, true);
    }

    items.clear();
}

void
vulkan_engine::consume_updated_render_assets()
{
    auto& items = glob::level::getr().get_dirty_render_assets_queue();

    for (auto& i : items)
    {
        glob::render_bridge::getr().render_ctor(*i, true);
    }

    items.clear();
}

void
vulkan_engine::consume_updated_shader_effects()
{
    auto& items = glob::level::getr().get_dirty_shader_effect_queue();

    for (auto& i : items)
    {
        glob::render_bridge::getr().render_ctor(*i, true);
    }

    items.clear();
}

}  // namespace agea