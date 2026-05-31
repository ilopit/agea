#include "engine/kryga_engine.h"

#include "engine/input_manager.h"
#include "engine/config.h"
#include "engine/engine_counters.h"
#include "engine/profiler.h"

#if KRG_HAS_IMGUI
#include "engine/console.h"
#include <backends/imgui_impl_sdl2.h>
#endif

#if KRG_EDITOR
#include "engine/editor_system.h"
#include "engine/ui.h"
#include "engine/editor.h"
#include "engine/private/ui/bake_editor.h"
#include "engine/private/ui/material_previewer.h"

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
#include <core/model_system.h>

#include <global_state/global_state.h>

#include <native/native_window.h>

#include <packages/root/model/assets/mesh.h>
#include <packages/base/model/components/mesh_component.h>
#include <packages/root/model/game_object.h>
#include <packages/root/model/assets/shader_effect.h>
#include <packages/base/model/camera_object.h>
#include <packages/base/model/player.h>

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
#include <vulkan_render/render_system.h>
#include <vulkan_render/types/vulkan_mesh_data.h>
#include <vulkan_render/types/vulkan_texture_data.h>
#include <vulkan_render/vk_descriptors.h>
#include <core/lightmap_manifest.h>

#include <vfs/io.h>

#include <animation/animation_system.h>

#include <physics/physics_system.h>

#include <game/game_system_manager.h>

#include <packages/base/model/components/destructible_mesh_component.h>

#include <gpu_types/gpu_vertex_types.h>

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
#include <cstdlib>

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

    app.add_option(
        "-l,--level", out.level, "Level to load on startup (default: light_sandbox_baked)");

    app.add_option("-d,--discovery",
                   out.discovery,
                   "Path for the RPC discovery JSON file (default: tmp/editor_rpc.json)");

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
    std::string dummy_s;
    app.add_option("-t,--run-for", dummy, "Run for specified duration then exit (0 = unlimited)")
        ->check(CLI::NonNegativeNumber);
    app.add_option(
        "-l,--level", dummy_s, "Level to load on startup (default: light_sandbox_baked)");
    app.add_option("-d,--discovery",
                   dummy_s,
                   "Path for the RPC discovery JSON file (default: tmp/editor_rpc.json)");

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

    m_initial_level = options.level;
    m_discovery_path = options.discovery;
    m_headless = options.headless;

    auto& gs = glob::glob_state();
    core::state_mutator__lua_api::set(gs);
    core::state_mutator__model::set(gs);

    gs.run_create();

    state_mutator__config::set(gs);
    state_mutator__render::set(gs);

    // input_manager is the only subsystem that truly can't run headless —
    // it hooks the OS event pump which tests don't drive. Everything else
    // (native_window, ui) works against a hidden SDL window.
    if (!m_headless)
    {
        state_mutator__input_manager::set(gs);
        glob::set_input_provider(glob::glob_state().get_input_manager());
    }
    state_mutator__native_window::set(gs);
    state_mutator__engine_counters::set(gs);
    state_mutator__queues::set(gs);
    state_mutator__render_bridge::set(gs);
    state_mutator__animation_system::set(gs);
    state_mutator__physics_system::set(gs);
    state_mutator__game_system_manager::set(gs);
#if KRG_EDITOR
    state_mutator__editor_system::set(gs);
#endif

    glob::glob_state().getr_animation_system().set_render_data_resolver(
        [](const utils::id& id) -> render::vulkan_render_data*
        { return glob::glob_state().getr_render().renderer.get_cache().objects.find_by_id(id); });

    glob::glob_state().getr_physics_system().init();
    glob::glob_state().getr_physics_system().build_ground_plane(-1000.0f);

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
        glob::glob_state().getr_editor_system().editor.init();
    }
#endif

    gs.run_init();

    render::render_device::construct_params rdc;
    // Build the swapchain from the saved config (present mode + frame count) so a
    // saved preference is honored at startup. The device reconciles these against
    // the surface's real capabilities and falls back if they aren't applicable.
    rdc.present = render_cfg.present;
    rdc.frames_in_flight = render_cfg.frames_in_flight;
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
    // Opt-in desktop fullscreen for present-latency testing (windowed FIFO pays
    // DWM composition latency; fullscreen takes the independent/hardware flip
    // path). KRYGA_FULLSCREEN=1 borderless desktop, =2 exclusive.
    if (!m_headless)
    {
        if (const char* fs = std::getenv("KRYGA_FULLSCREEN"))
        {
            rwc.fullscreen = std::atoi(fs);
        }
    }
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

    auto& device = glob::glob_state().getr_render().device;
    if (!device.construct(rdc))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

#if KRG_EDITOR
    glob::glob_state().getr_editor_system().ui.init();

    if (!m_headless)
    {
        ui::get_window<ui::bake_editor>()->init(vfs::rid("data://configs/bake.acfg"),
                                                vfs::rid("rtcache://bake.acfg"));
    }
#elif KRG_HAS_IMGUI
    if (!m_headless)
    {
        ImGui::CreateContext();
        ImGui_ImplSDL2_InitForVulkan(window->handle());
    }
#endif

#if KRG_HAS_IMGUI
    if (!m_headless)
    {
        m_console = std::make_unique<ui::editor_console>();
    }
#endif

    // Use the actual window size after SDL settled it — on Android fullscreen
    // the requested rwc.w/rwc.h is ignored and the surface is device-sized
    // (2280x1080 on Pixel 4 landscape). Passing the requested values here
    // produces a coord mismatch: viewport/ImGui draw into a 3200x1800 logical
    // space while the swapchain is only 2280x1080, collapsing everything into
    // the lower-left ~70% of the screen.
    auto actual_size = window->get_size();
    glob::glob_state().getr_render().renderer.init(
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

        std::optional<std::filesystem::path> disco_path;
        if (!m_discovery_path.empty())
        {
            std::filesystem::path p{m_discovery_path};
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

        rebuild_physics_static_world();
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
        glob::glob_state().getr_render().renderer.get_render_config().save_to_cache(
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

    glob::glob_state().getr_render().device.wait_for_fences();

#if KRG_HAS_IMGUI
    m_console.reset();
#endif

#if KRG_EDITOR
    glob::glob_state().getr_editor_system().ui.get_material_previewer().destroy();
    // Free the screenshot staging image before device.destruct() tears down the
    // VMA allocator (otherwise its lazily-created allocation outlives the
    // allocator -> VMA assertion, but only once a screenshot has been taken).
    glob::glob_state().getr_editor_system().screenshot.release();
#elif KRG_HAS_IMGUI
    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }
#endif

    glob::glob_state().getr_render().loader.clear_caches();

    glob::glob_state().getr_render().renderer.deinit();

    glob::glob_state().getr_render().device.destruct();

    if (auto* phys = glob::glob_state().get_physics_system())
    {
        phys->shutdown();
    }

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
        glob::glob_state().getr_render().renderer.draw_main();

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

        // Wait for the previous frame's render to finish BEFORE building the next
        // ImGui frame. ImGui's draw data is single-buffered and shared across the
        // main/render threads; the render thread reads it in update_ui (after a
        // vsync stall on vkAcquireNextImageKHR in FIFO). If we called NewFrame()
        // before this wait, the next frame's ImGui::NewFrame() could overwrite the
        // draw data the render thread is still consuming -> UI flicker/vanish in
        // FIFO (MAILBOX hid it because acquire doesn't stall, so the read always
        // won the race). First iteration is a no-op (m_render_done starts true).
        {
            std::unique_lock lock(m_render_mutex);
            m_main_cv.wait(lock, [this] { return m_render_done; });
        }

#if KRG_EDITOR
        {
            KRG_make_scope(ui_tick);
            KRG_PROFILE_SCOPE("UI");
            glob::glob_state().getr_editor_system().ui.new_frame(frame_time);
        }
#elif KRG_HAS_IMGUI
        {
            ImGuiIO& io = ImGui::GetIO();
            auto& vr = glob::glob_state().getr_render().renderer;
            io.DisplaySize = ImVec2((float)vr.width(), (float)vr.height());
            io.DeltaTime = frame_time;

            ImGui_ImplSDL2_NewFrame();
            io.DisplaySize = ImVec2((float)vr.width(), (float)vr.height());
            io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

            ImGui::NewFrame();
            if (m_console)
            {
                m_console->handle();
            }
            ImGui::Render();
        }
#endif
        {
            KRG_make_scope(tick);
            KRG_PROFILE_SCOPE("Tick");
            tick(frame_time);
        }
        // Render thread is done (waited above, before ui_tick) — all commands
        // executed and destructed. Safe to reclaim arena memory for next frame.
        glob::glob_state().getr_render_bridge().reset_arena();

        // Apply config edits the UI made during ui_tick. Topology changes
        // (e.g. render_scale.enabled) call vkDeviceWaitIdle and rebuild GPU
        // resources here, BEFORE the next frame's render starts.
        glob::glob_state().getr_render().renderer.apply_pending_render_config();

        {
            auto& ctrs = ::kryga::glob::glob_state().getr_engine_counters();
            auto& vr = glob::glob_state().getr_render().renderer;

            ctrs.all_draws.update(vr.get_all_draws());
            ctrs.culled_draws.update(vr.get_culled_draws());
            ctrs.objects.update(vr.get_cache().objects.get_actual_size());
        }

        {
            KRG_make_scope(consume_updates);
            KRG_PROFILE_SCOPE("ConsumeUpdates");

            update_cameras();
            glob::glob_state().getr_render().renderer.set_camera(m_camera_data);

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
    glob::glob_state().getr_render().renderer.draw_headless();
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
        int mode_now = static_cast<int>(glob::glob_state().getr_editor_system().editor.get_mode());
        if (mode_now != m_last_known_mode)
        {
            m_last_known_mode = mode_now;
            Json::Value note(Json::objectValue);
            note["mode"] = (mode_now == static_cast<int>(engine::editor_mode::playing))
                               ? std::string("play")
                               : std::string("edit");
            m_rpc_server->notify("engine.mode.changed", note);
        }

        auto sel_now = glob::glob_state().getr_editor_system().editor.get_selected();
        if (sel_now != m_last_known_selection)
        {
            m_last_known_selection = sel_now;
            Json::Value note(Json::objectValue);
            note["id"] = sel_now.valid() ? sel_now.str() : std::string();
            m_rpc_server->notify("model.selection.changed", note);
        }
    }

    glob::glob_state().getr_editor_system().editor.on_tick(dt);

    const bool playing =
        glob::glob_state().getr_editor_system().editor.get_mode() == engine::editor_mode::playing;

    if (playing)
    {
        glob::glob_state().getr_game_system_manager().tick_phase(game::game_phase::pre_physics, dt);

        if (auto lvl = glob::glob_state().getr_model().current_level)
        {
            lvl->tick(dt);
        }
    }

    // Notify VS Code of object-set changes. Polled rather than hooked into
    // level::spawn/unregister to keep core decoupled from RPC.
    if (m_rpc_server)
    {
        size_t new_count = 0;
        if (auto lvl = glob::glob_state().getr_model().current_level)
        {
            new_count = lvl->get_game_objects().get_items().size();
        }
        if (new_count != m_last_known_object_count)
        {
            m_last_known_object_count = new_count;
            Json::Value note(Json::objectValue);
            note["count"] = static_cast<Json::UInt64>(new_count);
            m_rpc_server->notify("model.scene.changed", note);
        }
    }
#else
    glob::glob_state().getr_game_system_manager().tick_phase(game::game_phase::pre_physics, dt);

    if (auto lvl = glob::glob_state().getr_model().current_level)
    {
        lvl->tick(dt);
    }
#endif

    if (auto* anim = glob::glob_state().get_animation_system())
    {
        anim->tick(dt);
    }

#if KRG_EDITOR
    if (!playing)
    {
        return;
    }
#endif

    if (auto* phys = glob::glob_state().get_physics_system())
    {
        phys->tick(dt);
    }

    glob::glob_state().getr_game_system_manager().tick_phase(game::game_phase::post_physics, dt);
    glob::glob_state().getr_game_system_manager().tick_phase(game::game_phase::late_update, dt);
}

bool
vulkan_engine::load_level(const utils::id& level_id)
{
    auto& lm = glob::glob_state().getr_model().levels;

    // Tear down the current level if any.  Destroy commands are enqueued
    // into the SPSC queue (arena-allocated).  Retire the current arena so
    // its memory survives until the render thread drains the commands.
    // The render thread calls schedule_to_delete with authoritative
    // m_current_frame_number — no cross-thread read.
#if KRG_EDITOR
    glob::glob_state().getr_editor_system().editor.set_selected(utils::id());
#endif
    if (auto* prev = glob::glob_state().getr_model().current_level)
    {
        unload_render_resources(*prev);
        lm.unload_level(*prev);

        glob::glob_state().getr_render_bridge().retire_arena();
    }

    auto result = lm.load_level(level_id);
    if (!result)
    {
        ALOG_FATAL("Nothing to do here!");
        return false;
    }

    glob::glob_state().getr_model().current_level = result;

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

                auto& loader = glob::glob_state().getr_render().loader;
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
        rb.render_cmd_transform(*i);
        i->set_dirty_transform(false);
    }

    items.clear();
}

void
vulkan_engine::update_cameras()
{
#if KRG_EDITOR
    auto& editor = glob::glob_state().getr_editor_system().editor;
    auto* cam = editor.get_active_camera();
    if (editor.get_mode() == engine::editor_mode::playing && cam)
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
        m_camera_data = editor.get_camera_data();
    }
#else
    // Game build — find first active camera in current level.
    base::camera_component* cam = nullptr;
    if (auto lvl = glob::glob_state().getr_model().current_level)
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
    auto level_id = m_initial_level.empty() ? AID("cubes") : AID(m_initial_level.c_str());
    if (level_id.valid())
    {
        load_level(level_id);
#if KRG_EDITOR
        // Editor-only debug helpers that populate the sandbox scene.
        glob::glob_state().getr_editor_system().editor.ev_spawn();
        glob::glob_state().getr_editor_system().editor.ev_lights();
#endif
    }

#if !KRG_EDITOR
    if (auto lvl = glob::glob_state().getr_model().current_level)
    {
        base::player::construct_params player_prms;
        auto player_obj = lvl->spawn_object<base::player>(AID("player_0"), player_prms);
        if (player_obj)
        {
            player_obj->get_camera()->set_active_camera(true);
            player_obj->get_camera()->set_perspective(
                60.f,
                glob::glob_state().getr_native_window().aspect_ratio(),
                (float)KGPU_znear,
                (float)KGPU_zfar);
        }

        for (auto& [id, obj] : lvl->get_game_objects().get_items())
        {
            if (auto go = obj->as<root::game_object>())
            {
                go->begin_play();
            }
        }
        glob::glob_state().getr_game_system_manager().on_begin_play();
    }
#endif
}

void
vulkan_engine::rebuild_physics_static_world()
{
    auto* ps = glob::glob_state().get_physics_system();
    if (!ps)
    {
        return;
    }

    auto* level = glob::glob_state().getr_model().current_level;
    if (!level)
    {
        // No level — keep whatever fallback (ground plane) is in place.
        return;
    }

    std::vector<physics::static_world_mesh> meshes;

    level->get_game_objects().call_on_items(
        [&meshes](root::game_object* go) -> bool
        {
            if (!go)
            {
                return true;
            }
            for (auto* comp : go->get_renderable_components())
            {
                // Skip destructibles — once broken they stop being collision
                // anyway, and keeping their pre-break body around would
                // collide with the chunks spawning on top of it.
                if (comp->castable_to<base::destructible_mesh_component>())
                {
                    continue;
                }

                auto* mc = comp->as<base::mesh_component>();
                if (!mc || !mc->get_mesh())
                {
                    continue;
                }

                auto* mesh = mc->get_mesh();
                auto vbuf = mesh->get_vertices_buffer().make_view<gpu::vertex_data>();
                auto ibuf = mesh->get_indices_buffer().make_view<gpu::uint>();
                if (vbuf.size() == 0 || ibuf.size() < 3)
                {
                    continue;
                }

                const glm::mat4 xf = mc->get_transform_matrix();

                physics::static_world_mesh entry;
                entry.vertices.reserve(vbuf.size());
                for (uint32_t i = 0; i < vbuf.size(); ++i)
                {
                    glm::vec4 p = xf * glm::vec4(vbuf.as()[i].position, 1.0f);
                    entry.vertices.emplace_back(p.x, p.y, p.z);
                }
                entry.indices.reserve(ibuf.size());
                for (uint32_t i = 0; i < ibuf.size(); ++i)
                {
                    entry.indices.push_back(ibuf.as()[i]);
                }
                meshes.push_back(std::move(entry));
            }
            return true;
        });

    if (meshes.empty())
    {
        ALOG_INFO("physics: no static meshes found in level, keeping ground plane");
        return;
    }

    ps->build_static_world(meshes);
    ALOG_INFO("physics: built static world from {} meshes", meshes.size());
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

    auto* pm = &glob::glob_state().getr_model().packages;
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
    auto& mq = glob::glob_state().getr_queues().get_model();
    auto& rb = glob::glob_state().getr_render_bridge();

    for (auto& i : mq.destroy_render)
    {
        rb.render_cmd_destroy(*i, true);
    }
    mq.destroy_render.clear();

    // TODO: when both a dependency (e.g. shader_effect) and its dependent (e.g. material) are dirty
    // in the same frame, render_cmd_build on the dependent will rebuild the dependency via
    // traversal, then the dependency's own queue entry triggers a redundant rebuild. Consider a
    // per-frame "already processed" flag to skip entries that were already rebuilt during
    // dependency traversal.
    for (auto& i : mq.dirty_render)
    {
        rb.render_cmd_build(*i, true);
    }
    mq.dirty_render.clear();
}

}  // namespace kryga
