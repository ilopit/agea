#include "engine/kryga_engine.h"

#include "engine/input_manager.h"
#include "engine/config.h"
#include "engine/engine_counters.h"
#include "engine/profiler.h"

#if KRG_EDITOR
#include "engine/ui.h"
#include "engine/editor.h"
#include "engine/private/ui/bake_editor.h"

#include <rpc/rpc_server.h>
#include <rpc/rpc_log_sink.h>

#include "engine/private/engine_rpc.h"

#include <project_paths/project_paths.h>

#include <json/json.h>
#endif

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

#include <core/queues.h>
#include <render_bridge/render_bridge.h>
#include <render_bridge/render_commands_common.h>

#include <vfs/vfs.h>

#include <vulkan_render/kryga_render.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/types/vulkan_mesh_data.h>
#include <vulkan_render/types/vulkan_texture_data.h>
#include <vulkan_render/vk_descriptors.h>
#include <core/lightmap_manifest.h>

#include <vfs/io.h>

#include <animation/animation_system.h>

#include <utils/kryga_log.h>
#include <utils/process.h>
#include <utils/clock.h>

#include <sol2_unofficial/sol.h>

#include <CLI/CLI.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <algorithm>
#include <chrono>
#include <thread>

namespace kryga
{

// ============================================================================
// Startup Options
// ============================================================================

bool
startup_options::parse(int argc, char** argv, startup_options& out)
{
    out = {};

    CLI::App app{"Kryga Engine"};

    app.add_option("-t,--run-for",
                   out.run_for_seconds,
                   "Run for specified duration then exit (0 = unlimited)")
        ->check(CLI::NonNegativeNumber);

    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
        out.show_help = (e.get_exit_code() == 0);  // --help returns 0
        if (!out.show_help)
        {
            ALOG_ERROR("{}", e.what());
        }
        return false;
    }

    return true;
}

void
startup_options::print_help(const char* program_name)
{
    CLI::App app{"Kryga Engine"};
    float dummy = 0.f;
    app.add_option("-t,--run-for", dummy, "Run for specified duration then exit (0 = unlimited)")
        ->check(CLI::NonNegativeNumber);

    ALOG_INFO("{}", app.help());
}

vulkan_engine::vulkan_engine()
#if KRG_EDITOR
    : m_rpc_server(std::make_unique<rpc::rpc_server>())
#endif
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

    m_run_for_seconds = options.run_for_seconds;
    if (m_run_for_seconds > 0.f)
    {
        ALOG_INFO("Run duration limit: {} seconds", m_run_for_seconds);
    }

    m_headless = options.headless;

    auto& gs = glob::glob_state();
    core::state_mutator__lua_api::set(gs);
    core::state_mutator__caches::set(gs);
    core::state_mutator__reflection_manager::set(gs);
    core::state_mutator__package_manager::set(gs);

    gs.schedule_action(gs::state::state_stage::create,
                       [](gs::state& s)
                       {
                           core::state_mutator__level_manager::set(s);
                           core::state_mutator__id_generator::set(s);
                       });

    gs.run_create();

    state_mutator__config::set(gs);
#if KRG_EDITOR
    state_mutator__game_editor::set(gs);
#endif
    // input_manager is the only subsystem that truly can't run headless —
    // it hooks the OS event pump which tests don't drive. Everything else
    // (native_window, ui) works against a hidden SDL window.
    if (!m_headless)
    {
        state_mutator__input_manager::set(gs);
        glob::set_input_provider(glob::glob_state().get_input_manager());
    }
    state_mutator__render_device::set(gs);
    state_mutator__vulkan_render::set(gs);
    state_mutator__vulkan_render_loader::set(gs);
#if KRG_EDITOR
    state_mutator__ui::set(gs);
#endif
    state_mutator__native_window::set(gs);
    state_mutator__engine_counters::set(gs);
    state_mutator__queues::set(gs);
    state_mutator__render_bridge::set(gs);
    state_mutator__animation_system::set(gs);

    glob::glob_state().getr_animation_system().set_render_data_resolver(
        [](const utils::id& id) -> render::vulkan_render_data*
        { return glob::glob_state().getr_vulkan_render().get_cache().objects.find_by_id(id); });

    gs.run_connect();
    init_default_scripting();

    glob::glob_state().get_config()->load(vfs::rid("data://configs/kryga.acfg"));

    if (!m_headless)
    {
        glob::glob_state().get_input_manager()->load_actions(
            vfs::rid("data://configs/inputs.acfg"));
    }

    // Load render config (with rtcache fallback for session state)
    render::render_config render_cfg;
    render_cfg.load_with_cache(vfs::rid("data://configs/render.acfg"),
                               vfs::rid("rtcache://render.acfg"));

#if KRG_EDITOR
    if (!m_headless)
    {
        // game_editor::init registers input actions — requires input_manager, which
        // headless mode skips
        glob::glob_state().get_game_editor()->init();
    }
#endif

    gs.schedule_action(gs::state::state_stage::init,
                       [](kryga::gs::state& s) { s.get_pm()->init(); });
    gs.run_init();

    render::render_device::construct_params rdc;
    uint32_t render_w = 1600 * 2;
    uint32_t render_h = 900 * 2;

    if (m_headless)
    {
        render_w = 512;
        render_h = 512;
    }

    // Always construct a window. In headless mode it's hidden — ImGui/UI still
    // need a valid SDL_Window* but we don't want anything appearing on screen.
    native_window::construct_params rwc;
    rwc.w = render_w;
    rwc.h = render_h;
    rwc.hidden = m_headless;
    auto window = glob::glob_state().get_native_window();
    if (!window->construct(rwc))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    if (m_headless)
    {
        rdc.headless = true;
        rdc.width = render_w;
        rdc.height = render_h;
    }
    else
    {
        // Headless render_device ignores the window; windowed mode binds to it
        // for swapchain/surface creation.
        rdc.window = window->handle();
    }

    auto device = glob::glob_state().get_render_device();
    if (!device->construct(rdc))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

#if KRG_EDITOR
    glob::glob_state().getr_ui().init();

    if (!m_headless)
    {
        // Load bake config (with rtcache fallback for session state) — editor-only
        // feature, skip in headless
        ui::get_window<ui::bake_editor>()->init(vfs::rid("data://configs/bake.acfg"),
                                                vfs::rid("rtcache://bake.acfg"));
    }
#endif

    // Use the actual window size after SDL settled it — on Android fullscreen
    // the requested rwc.w/rwc.h is ignored and the surface is device-sized
    // (2280x1080 on Pixel 4 landscape). Passing the requested values here
    // produces a coord mismatch: viewport/ImGui draw into a 3200x1800 logical
    // space while the swapchain is only 2280x1080, collapsing everything into
    // the lower-left ~70% of the screen.
    auto actual_size = window->get_size();
    glob::glob_state().getr_vulkan_render().init(
        (uint32_t)actual_size.w, (uint32_t)actual_size.h, render_cfg);

    init_default_resources();

    if (!m_headless)
    {
        // init_scene hardcodes a level + uses native_window.aspect_ratio; tests load
        // their own level and set their own camera
        init_scene();

#if KRG_EDITOR
        // rpc: bind on a free port (OS-picked), publish discovery file
        // for the VS Code extension to find. Single-engine assumption per
        // project (multi-engine could key by PID later).
        engine_private::register_rpc_handlers(*this, *m_rpc_server);

        // Discovery file lives at <source_root>/tmp/editor_rpc.json — a stable
        // path independent of build config so VS Code (and other tooling) can
        // find a running engine without knowing whether it's a Debug or
        // Release build. Falls back to tmp:// for non-dev layouts where
        // source_root isn't known.
        std::optional<std::filesystem::path> disco_path;
        if (auto layout = kryga::paths::resolve(); layout && !layout->source_root.empty())
        {
            auto p = layout->source_root / "tmp" / "editor_rpc.json";
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
            disco_path = std::move(p);
        }
        else
        {
            disco_path = glob::glob_state().getr_vfs().real_path(vfs::rid("tmp://editor_rpc.json"));
        }
        if (disco_path.has_value())
        {
            m_rpc_server->start(0, disco_path->string());

            m_rpc_log_sink = std::make_shared<rpc::rpc_log_sink>(*m_rpc_server);
            spdlog::default_logger()->sinks().push_back(m_rpc_log_sink);
        }
        else
        {
            ALOG_WARN(
                "rpc: could not resolve discovery file path — "
                "RPC server not started");
        }
#endif
    }

    ALOG_INFO("Initialization completed");
    return true;
}

void
vulkan_engine::cleanup()
{
    // Save session state to rtcache (non-headless only — headless tests don't touch session cfg)
    if (!m_headless)
    {
        glob::glob_state().getr_vulkan_render().get_render_config().save_to_cache(
            vfs::rid("rtcache://render.acfg"));
#if KRG_EDITOR
        ui::get_window<ui::bake_editor>()->save_config();
#endif

        glob::set_input_provider(nullptr);
    }

#if KRG_EDITOR
    // Detach RPC log sink BEFORE stopping the server so a late spdlog write
    // doesn't notify into a dead server.
    if (m_rpc_log_sink)
    {
        auto& sinks = spdlog::default_logger()->sinks();
        sinks.erase(std::remove(sinks.begin(), sinks.end(), m_rpc_log_sink), sinks.end());
        m_rpc_log_sink.reset();
    }
    m_rpc_server->stop();
#endif

    glob::glob_state().get_render_device()->wait_for_fences();

    glob::glob_state().get_vulkan_render_loader()->clear_caches();

    glob::glob_state().getr_vulkan_render().deinit();

    glob::glob_state().get_render_device()->destruct();

    glob::glob_state_reset();
}

// TODO: Streaming command execution optimization.
// Currently this is a batch model: render thread sleeps until main enqueues ALL
// commands, then drains everything at once. The SPSC queue and arena support
// concurrent push/pop, so render could drain incrementally while main is still
// enqueueing. To implement:
//   1. Render thread spins on try_pop() instead of CV wait (with yield backoff).
//   2. Main enqueues a draw_fence_cmd as the last command per frame.
//   3. draw_fence_cmd::execute() calls draw_main() + signals m_main_cv.
//   4. Remove m_render_work_ready — render is always running.
//   5. Arena is safe: it only grows forward during a frame, render reads earlier offsets.
// Trade-off: burns CPU when idle (spinning) vs current zero-cost CV sleep.
// Consider hybrid: spin N iterations, then fall back to CV wait.
void
vulkan_engine::render_thread_func()
{
    while (true)
    {
        {
            std::unique_lock lock(m_render_mutex);
            m_render_cv.wait(lock, [this] { return m_render_work_ready || m_render_shutdown; });
            if (m_render_shutdown)
            {
                break;
            }
            m_render_work_ready = false;
        }

        glob::glob_state().getr_render_bridge().drain_queue();
        glob::glob_state().getr_vulkan_render().draw_main();

        {
            std::lock_guard lock(m_render_mutex);
            m_render_done = true;
        }
        m_main_cv.notify_one();
    }
}

void
vulkan_engine::run()
{
    // run() pumps SDL events and calls ui->new_frame → requires an input_manager
    // and an active SDL event loop. Headless mode skips input_manager, so tests
    // must drive rendering via tick_headless() instead.
    KRG_check(!m_headless,
              "vulkan_engine::run() is not supported in headless mode — use tick_headless()");

    float frame_time = 1.f / glob::glob_state().get_config()->fps_lock;
    const std::chrono::microseconds frame_time_int(1000000 /
                                                   glob::glob_state().get_config()->fps_lock);

    float total_elapsed = 0.f;

    m_render_thread = std::thread(&vulkan_engine::render_thread_func, this);

    // main loop
    for (;;)
    {
        // Check run duration limit
        if (m_run_for_seconds > 0.f && total_elapsed >= m_run_for_seconds)
        {
            ALOG_INFO("Run duration limit reached ({} seconds), exiting.", m_run_for_seconds);
            break;
        }
#if KRG_EDITOR
        if (m_shutdown_requested.load(std::memory_order_relaxed))
        {
            ALOG_INFO("Shutdown requested via RPC, exiting.");
            break;
        }
#endif

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
#if KRG_EDITOR
        {
            KRG_make_scope(ui_tick);
            KRG_PROFILE_SCOPE("UI");
            glob::glob_state().get_ui()->new_frame(frame_time);
        }
#endif
        {
            KRG_make_scope(tick);
            KRG_PROFILE_SCOPE("Tick");
            tick(frame_time);
        }
        // Wait for previous frame's render to finish before touching render state.
        // First iteration is a no-op (m_render_done starts true).
        {
            std::unique_lock lock(m_render_mutex);
            m_main_cv.wait(lock, [this] { return m_render_done; });
        }

        // Render thread is done — all commands executed and destructed.
        // Safe to reclaim arena memory for next frame.
        glob::glob_state().getr_render_bridge().reset_arena();

        // Apply config edits the UI made during ui_tick. Topology changes
        // (e.g. render_scale.enabled) call vkDeviceWaitIdle and rebuild GPU
        // resources here, BEFORE the next frame's render starts.
        glob::glob_state().getr_vulkan_render().apply_pending_render_config();

        {
            auto& ctrs = ::kryga::glob::glob_state().getr_engine_counters();
            auto& vr = glob::glob_state().getr_vulkan_render();

            ctrs.all_draws.update(vr.get_all_draws());
            ctrs.culled_draws.update(vr.get_culled_draws());
            ctrs.objects.update(vr.get_cache().objects.get_actual_size());
        }

        {
            KRG_make_scope(consume_updates);
            KRG_PROFILE_SCOPE("ConsumeUpdates");

            update_cameras();
            glob::glob_state().getr_vulkan_render().set_camera(m_camera_data);

            consume_updated_render();
            consume_updated_transforms();
        }

        // Signal render thread to draw this frame
        {
            std::lock_guard lock(m_render_mutex);
            m_render_work_ready = true;
            m_render_done = false;
        }
        m_render_cv.notify_one();

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

    // Wait for any in-flight render to finish, then shutdown render thread
    {
        std::unique_lock lock(m_render_mutex);
        m_main_cv.wait(lock, [this] { return m_render_done; });
        m_render_shutdown = true;
    }
    m_render_cv.notify_one();
    m_render_thread.join();
}
void
vulkan_engine::tick_headless()
{
    // Process dirty-render items queued by level/package load
    consume_updated_render();
    consume_updated_transforms();

    glob::glob_state().getr_render_bridge().drain_queue();
    glob::glob_state().getr_vulkan_render().draw_headless();
    glob::glob_state().getr_render_bridge().reset_arena();
}

#if KRG_EDITOR
void
vulkan_engine::queue_main_action(std::function<void()> a)
{
    std::lock_guard<std::mutex> lk(m_rpc_action_mutex);
    m_rpc_actions.push_back(std::move(a));
}

bool
vulkan_engine::wait_main_action(std::function<void()> a, std::chrono::milliseconds timeout)
{
    auto p = std::make_shared<std::promise<void>>();
    auto fut = p->get_future();
    queue_main_action(
        [p, work = std::move(a)]() mutable
        {
            try
            {
                work();
                p->set_value();
            }
            catch (...)
            {
                p->set_exception(std::current_exception());
            }
        });
    if (fut.wait_for(timeout) != std::future_status::ready)
    {
        return false;
    }
    fut.get();
    return true;
}

void
vulkan_engine::drain_main_actions()
{
    std::vector<std::function<void()>> local;
    {
        std::lock_guard<std::mutex> lk(m_rpc_action_mutex);
        local.swap(m_rpc_actions);
    }
    for (auto& a : local)
    {
        a();
    }
}
#endif

void
vulkan_engine::tick(float dt)
{
#if KRG_EDITOR
    drain_main_actions();

    if (m_rpc_server)
    {
        int mode_now = static_cast<int>(glob::glob_state().get_game_editor()->get_mode());
        if (mode_now != m_last_known_mode)
        {
            m_last_known_mode = mode_now;
            Json::Value note(Json::objectValue);
            note["mode"] = (mode_now == static_cast<int>(engine::editor_mode::playing))
                               ? std::string("play")
                               : std::string("edit");
            m_rpc_server->notify("engine.mode.changed", note);
        }
    }

    glob::glob_state().get_game_editor()->on_tick(dt);
    if (glob::glob_state().get_game_editor()->get_mode() == engine::editor_mode::playing)
    {
        if (auto lvl = glob::glob_state().get_current_level())
        {
            lvl->tick(dt);
        }
    }

    // Notify VS Code of object-set changes. Polled rather than hooked into
    // level::spawn/unregister to keep core decoupled from RPC.
    if (m_rpc_server)
    {
        size_t new_count = 0;
        if (auto lvl = glob::glob_state().get_current_level())
        {
            new_count = lvl->get_game_objects().get_items().size();
        }
        if (new_count != m_last_known_object_count)
        {
            m_last_known_object_count = new_count;
            Json::Value note(Json::objectValue);
            note["count"] = static_cast<Json::UInt64>(new_count);
            m_rpc_server->notify("scene.changed", note);
        }
    }
#else
    // Game build — always playing, level always ticks.
    if (auto lvl = glob::glob_state().get_current_level())
    {
        lvl->tick(dt);
    }
#endif

    if (auto* anim = glob::glob_state().get_animation_system())
    {
        anim->tick(dt);
    }
}

bool
vulkan_engine::load_level(const utils::id& level_id)
{
    auto lm = glob::glob_state().get_lm();
    auto cs = glob::glob_state().get_class_set();
    auto is = glob::glob_state().get_instance_set();

    // Tear down the current level if any.  Destroy commands are enqueued
    // into the SPSC queue (arena-allocated).  Retire the current arena so
    // its memory survives until the render thread drains the commands.
    // The render thread calls schedule_to_delete with authoritative
    // m_current_frame_number — no cross-thread read.
#if KRG_EDITOR
    glob::glob_state().get_game_editor()->set_selected(utils::id());
#endif
    if (auto* prev = glob::glob_state().get_current_level())
    {
        unload_render_resources(*prev);
        lm->unload_level(*prev);

        glob::glob_state().getr_render_bridge().retire_arena();
    }

    auto result = lm->load_level(level_id);
    if (!result)
    {
        ALOG_FATAL("Nothing to do here!");
        return false;
    }

    core::state_mutator__current_level::set(*result, glob::glob_state());

    // Create lightmap texture if level references baked data
    if (result->has_lightmap_ref())
    {
        auto manifest = std::make_unique<core::lightmap_manifest>();
        if (manifest->load(result->get_lightmap_manifest_rid()))
        {
            std::vector<uint8_t> lm_data;
            if (vfs::load_file(result->get_lightmap_bin_rid(), lm_data) && !lm_data.empty())
            {
                utils::buffer lm_buf(lm_data.size());
                std::memcpy(lm_buf.data(), lm_data.data(), lm_data.size());

                auto& loader = glob::glob_state().getr_vulkan_render_loader();
                auto lm_tex_id = AID((result->get_id().str() + "_lightmap").c_str());
                auto* tex = loader.update_or_create_texture(lm_tex_id,
                                                            lm_buf,
                                                            manifest->atlas_width,
                                                            manifest->atlas_height,
                                                            VK_FORMAT_R16G16B16A16_SFLOAT,
                                                            render::texture_format::rgba16f);

                if (tex)
                {
                    result->set_lightmap(tex->get_bindless_index(), std::move(manifest));
                }
            }
        }
    }

    return true;
}

bool
vulkan_engine::unload_render_resources(core::level& l)
{
    auto& cs = l.get_local_cache();

    for (auto& t : cs.objects.get_items())
    {
        glob::glob_state().getr_render_bridge().render_cmd_destroy(*t.second, true);
    }

    return true;
}

bool
vulkan_engine::unload_render_resources(core::package& l)
{
    auto& cs = l.get_local_cache();

    for (auto& t : cs.objects.get_items())
    {
        glob::glob_state().getr_render_bridge().render_cmd_destroy(*t.second, true);
    }

    return true;
}

void
vulkan_engine::consume_updated_transforms()
{
    auto& items = glob::glob_state().getr_queues().get_model().dirty_transforms;

    if (items.empty())
    {
        return;
    }

    auto& rb = glob::glob_state().getr_render_bridge();

    for (auto& i : items)
    {
        auto r = i->get_owner()->get_components(i->get_order_idx());

        for (auto& obj : r)
        {
            if (auto m = obj.as<base::mesh_component>())
            {
                if (m->get_render_built())
                {
                    auto* cmd = rb.alloc_cmd<update_transform_cmd>();
                    cmd->id = m->get_id();
                    cmd->transform = m->get_transform_matrix();
                    cmd->normal_matrix = m->get_normal_matrix();
                    cmd->position = glm::vec3(m->get_world_position());

                    auto scale = m->get_scale();
                    float max_s =
                        glm::max(glm::max(glm::abs(scale.x), glm::abs(scale.y)), glm::abs(scale.z));
                    cmd->bounding_radius = m->get_base_bounding_radius() * max_s;

                    const auto& lc = m->get_mesh()->get_local_centroid();
                    glm::vec4 wc = m->get_transform_matrix() * glm::vec4(lc.x, lc.y, lc.z, 1.0f);
                    cmd->bounding_sphere_center = glm::vec3(wc);

                    rb.enqueue_cmd(cmd);
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
#if KRG_EDITOR
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
#else
    // Game build — find first active camera in current level.
    base::camera_component* cam = nullptr;
    if (auto lvl = glob::glob_state().get_current_level())
    {
        for (auto& [id, obj] : lvl->get_game_objects().get_items())
        {
            auto go = obj->as<root::game_object>();
            if (!go)
            {
                continue;
            }
            for (auto c : go->get_renderable_components())
            {
                if (auto cc = c->as<base::camera_component>())
                {
                    if (cc->is_active_camera())
                    {
                        cam = cc;
                        break;
                    }
                }
            }
            if (cam)
            {
                break;
            }
        }
    }
    KRG_check(cam, "Game build requires an active camera in the level.");
    float aspect = glob::glob_state().getr_native_window().aspect_ratio();
    cam->set_aspect_ratio(aspect);
    m_camera_data.projection = cam->get_perspective();
    m_camera_data.inv_projection = cam->get_inv_projection();
    m_camera_data.view = cam->get_view();
    m_camera_data.position = cam->get_owner()->get_position();
#endif
}

void
vulkan_engine::init_default_resources()
{
    // plane_mesh is now created by vulkan_render::prepare_system_resources()
}

void
vulkan_engine::init_scene()
{
    auto level_id = AID("light_sandbox_baked");
    if (level_id.valid())
    {
        load_level(level_id);
#if KRG_EDITOR
        // Editor-only debug helpers that populate the sandbox scene.
        glob::glob_state().getr_game_editor().ev_spawn();
        glob::glob_state().getr_game_editor().ev_lights();
#endif
    }

    if (auto lvl = glob::glob_state().get_current_level())
    {
        base::camera_object::construct_params co_prms;
        auto cam_obj = lvl->spawn_object<base::camera_object>(AID("play_camera"), co_prms);
        if (cam_obj)
        {
            cam_obj->get_camera_component()->set_active_camera(true);
            cam_obj->get_camera_component()->set_perspective(
                60.f,
                glob::glob_state().getr_native_window().aspect_ratio(),
                (float)KGPU_znear,
                (float)KGPU_zfar);
        }

#if !KRG_EDITOR
        // Game build is always "playing" — fire begin_play on level objects.
        for (auto& [id, obj] : lvl->get_game_objects().get_items())
        {
            if (auto go = obj->as<root::game_object>())
            {
                go->begin_play();
            }
        }
#endif
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
        "pm",
        sol::no_constructor,
        "get_package",
        [pm](const char* id) -> core::package* { return pm->get_package(AID(id)); },
        "load",
        [pm](const char* id) -> bool { return pm->load_package(AID(id)); });

    gs.create_box_with_obj("lua_pm", std::move(lua_pm));
}

void
vulkan_engine::consume_updated_render()
{
    auto& items = glob::glob_state().getr_queues().get_model().dirty_render;

    // TODO: when both a dependency (e.g. shader_effect) and its dependent (e.g. material) are dirty
    // in the same frame, render_cmd_build on the dependent will rebuild the dependency via
    // traversal, then the dependency's own queue entry triggers a redundant rebuild. Consider a
    // per-frame "already processed" flag to skip entries that were already rebuilt during
    // dependency traversal.
    for (auto& i : items)
    {
        glob::glob_state().getr_render_bridge().render_cmd_build(*i, true);
    }

    items.clear();
}

}  // namespace kryga
