#include "engine/agea_engine.h"

#include "engine/ui.h"
#include "engine/input_manager.h"
#include "engine/editor.h"
#include "engine/config.h"
#include "engine/engine_counters.h"

#include "vulkan_render/vulkan_render.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vk_descriptors.h"

#include <model/caches/components_cache.h>
#include <model/caches/materials_cache.h>
#include <model/caches/meshes_cache.h>
#include <model/caches/objects_cache.h>
#include <model/caches/textures_cache.h>
#include <model/caches/game_objects_cache.h>
#include <model/caches/caches_map.h>
#include <model/caches/empty_objects_cache.h>

#include <model/reflection/lua_api.h>
#include <model/components/mesh_component.h>
#include <model/game_object.h>
#include <model/assets/shader_effect.h>
#include <model/level_manager.h>
#include <model/level.h>
#include <model/package.h>
#include <model/package_manager.h>
#include <model/id_generator.h>

#include <native/native_window.h>

#include <utils/agea_log.h>
#include <utils/process.h>
#include <utils/clock.h>

#include <imgui.h>

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
    , m_scene(std::make_unique<scene_builder>())
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
    glob::level::create(*m_registry);
    glob::native_window::create(*m_registry);
    glob::package_manager::create(*m_registry);
    glob::lua_api::create(*m_registry);
    glob::vulkan_render::create(*m_registry);
    glob::id_generator::create(*m_registry);
    glob::engine_counters::create(*m_registry);

    glob::init_global_caches(*m_registry);

    glob::resource_locator::get()->init_local_dirs();
    auto cfgs_folder = glob::resource_locator::get()->resource_dir(category::configs);

    utils::path main_config = cfgs_folder / "agea.acfg";
    glob::config::get()->load(main_config);

    utils::path input_config = cfgs_folder / "inputs.acfg";
    glob::input_manager::get()->load_actions(input_config);

    ::agea::reflection::entry::set_up();

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
    auto result = model::level_manager::load_level_id(
        *glob::level::get(), glob::config::get()->level, glob::class_objects_cache_set::get(),
        glob::objects_cache_set::get());

    if (!result)
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    if (!prepare_for_rendering(*glob::level::get()))
    {
        ALOG_LAZY_ERROR;
        return false;
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
        auto obj_data = i->get_render_object_data();
        if (obj_data)
        {
            obj_data->gpu_data.model_matrix = i->get_transofrm_matrix();
            i->set_dirty_transform(false);
            glob::vulkan_render::getr().schedule_game_data_gpu_transfer(obj_data);
        }
    }

    items.clear();
}

void
vulkan_engine::update_cameras()
{
    m_camera_data = glob::game_editor::get()->get_camera_data();
}

void
vulkan_engine::init_scene()
{
    load_level(glob::config::get()->level);

    for (int x = 0; x < 3; ++x)
    {
        for (int y = 0; y < 3; ++y)
        {
            for (int z = 0; z < 3; ++z)
            {
                auto id = std::format("obj_{}_{}_{}", x, y, z);
                auto p =
                    glob::level::getr().spawn_object<model::game_object>(AID("decor"), AID(id));

                p->get_root_component()->set_position(model::vec3{x * 10.f, y * 10.f, z * 10.f});

                m_scene->prepare_for_rendering(*p, true);
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
        auto obj_data = (render::object_data*)i->get_render_object_data();

        if (obj_data)
        {
            m_scene->prepare_for_rendering(*i, false);
        }
    }

    items.clear();
}

void
vulkan_engine::consume_updated_render_assets()
{
    auto& items = glob::level::getr().get_dirty_render_assets_queue();

    for (auto& i : items)
    {
        m_scene->prepare_for_rendering(*i, false);
    }

    items.clear();
}

void
vulkan_engine::consume_updated_shader_effects()
{
    auto& items = glob::level::getr().get_dirty_shader_effect_queue();

    for (auto& i : items)
    {
        m_scene->prepare_for_rendering(*i, false);
    }

    items.clear();
}

bool
vulkan_engine::prepare_for_rendering(model::package& p)
{
    auto& cs = p.get_objects();

    for (auto& o : cs)
    {
        if (!m_scene->prepare_for_rendering(*o, true))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    return true;
}

bool
vulkan_engine::prepare_for_rendering(model::level& p)
{
    auto& ids = p.get_package_ids();

    for (auto& id : ids)
    {
        auto p = glob::package_manager::getr().get_package(id);
        prepare_for_rendering(*p);
    }

    auto& cs = p.get_game_objects();

    for (auto& o : cs.get_items())
    {
        if (!m_scene->prepare_for_rendering(*o.second, true))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    return true;
}

void
vulkan_engine::compile_all_shaders()
{
}

}  // namespace agea
