#include "engine/kryga_engine.h"

#include "engine/input_manager.h"
#include "engine/config.h"
#include "engine/engine_counters.h"
#include "engine/profiler.h"

#if KRG_HAS_IMGUI
#include "engine/console.h"
#include <backends/imgui_impl_sdl2.h>
#endif

#if KRG_HAS_EDITOR
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
#include <render_translator/render_commands.h>
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

#include <render_translator/render_translator.h>
#include <render_translator/render_command.h>
#include <render_translator/render_command_processor.h>
#include <render_translator/render_convert.h>

#include <audio_bridge/audio_bridge.h>
#include <audio_bridge/audio_message_processor.h>

#include <physics_bridge/physics_bridge.h>
#include <physics_bridge/physics_command_processor.h>

#include <core/audio_message.h>

#include <vfs/vfs.h>

#include <vulkan_render/kryga_render.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/render_system.h>
#include <core/subsystem_queues.h>
#include <vulkan_render/render_thread.h>
#include <vulkan_render/types/vulkan_mesh_data.h>
#include <vulkan_render/types/vulkan_texture_data.h>
#include <vulkan_render/vk_descriptors.h>
#include <core/lightmap_manifest.h>

#include <vfs/io.h>

#include <animation/animation_system.h>

#include <audio/audio_system.h>

#include <physics/physics_system.h>

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
#include <cstring>
#include <unordered_map>
#include <vector>

namespace kryga
{

// Defined in crash_handler.cpp — installs an unhandled-exception minidump writer.
void
install_crash_handler();

// create_lightmap_cmd / destroy_lightmap_cmd now live in
// render_translator/render_commands.h (relocated for central tagged dispatch).

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
#if KRG_HAS_EDITOR
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
    // Write a minidump on any unhandled crash (catches timing races a debugger
    // would perturb away). No-op on non-Windows.
    install_crash_handler();

    // Main thread grants itself render-state access for single-threaded setup
    // (renderer.init stages default textures, etc.). It hands this off before the
    // streaming loop starts (see run()) and reclaims it after the loop ends.
    render::set_render_access(true);
    // Main is also the model/build thread for the whole process — it owns the
    // content allocator (reserve/free/tick). Granted once and never handed off.
    render::set_model_access(true);

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
    state_mutator__subsystem_queues::set(gs);

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
    state_mutator__render_translator::set(gs);
    state_mutator__audio_bridge::set(gs);
    state_mutator__physics_bridge::set(gs);
    state_mutator__animation_system::set(gs);
    state_mutator__physics_system::set(gs);
    state_mutator__audio_system::set(gs);
#if KRG_HAS_EDITOR
    state_mutator__editor_system::set(gs);
#endif

    glob::glob_state().getr_animation_system().set_render_data_resolver(
        [](render::types::render_object_handle h) -> render::vulkan_render_data*
        { return glob::glob_state().getr_render().renderer.get_cache().get_object(h); });

    glob::glob_state().getr_physics_system().init();
    // Bind the bridge's static-collider allocator to physics_system's BodyID storage
    // (render_translator-style split): the allocator grows + indexes that storage.
    glob::glob_state().getr_physics_bridge().bind_static_storage(
        glob::glob_state().getr_physics_system());
    glob::glob_state().getr_physics_system().build_ground_plane(-1000.0f);

    // Build the consumer-side processors once, owned by the engine for the whole run.
    // The same instance is driven either by its worker thread (threaded — passed to
    // m_threads.start()) or inline (headless — tick_headless / consume_updated_audio);
    // the engine is never both, so there's exactly one driver per processor.
    {
        auto& q = glob::glob_state().getr_subsystem_queues();
        m_physics_processor = std::make_unique<physics_command_processor>(
            glob::glob_state().getr_physics_system(), q.physics.in, q.physics.out);

        auto* as = glob::glob_state().get_audio_system();
        KRG_check(as, "audio_system must exist before building its processor");
        m_audio_processor = std::make_unique<audio_message_processor>(as->renderer);

        // Render consumer: binds the renderer + loader refs (both already exist; the
        // processor only stores them — it's used later, on the render thread or inline).
        m_render_processor = std::make_unique<render_command_processor>(
            glob::glob_state().getr_render().renderer, glob::glob_state().getr_render().loader);
    }

    gs.run_connect();
    init_default_scripting();

    // Runtime overlay: the rtcache copy (session state, e.g. last opened level)
    // wins when present; otherwise the committed base config. Keeps the committed
    // default clean while letting the running editor remember its level.
    auto* engine_cfg = glob::glob_state().get_config();
    engine_cfg->bind(vfs::rid("data://configs/kryga.acfg"), vfs::rid("rtcache://kryga.acfg"));
    engine_cfg->load();

    // Startup level precedence: an explicit CLI -l wins; otherwise fall back to the
    // config's `level` field; if neither is set, init_scene uses its built-in default.
    if (m_initial_level.empty() && glob::glob_state().get_config()->level.valid())
    {
        m_initial_level = glob::glob_state().get_config()->level.str();
    }

    if (!m_headless)
    {
        glob::glob_state().get_input_manager()->load_actions(
            vfs::rid("data://configs/inputs.acfg"));
    }

    // Load render config (with rtcache fallback for session state)
    render::render_config render_cfg;
    render_cfg.bind(vfs::rid("data://configs/render.acfg"), vfs::rid("rtcache://render.acfg"));
    render_cfg.load();

#if KRG_HAS_EDITOR
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

#if KRG_HAS_EDITOR
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

    // Bind the model-side allocators to their render-side storages now that the
    // loader + render cache exist, then preallocate the object slot pool: the
    // allocator's preallocate grows the bound render storage too, synchronously.
    auto& rb = glob::glob_state().getr_render_translator();
    rb.bind_content_storages();
    rb.objects_alloc().preallocate(glob::glob_state().get_config()->object_pool_size);

    init_default_resources();

    if (!m_headless)
    {
        // init_scene hardcodes a level + uses native_window.aspect_ratio; tests load
        // their own level and set their own camera
        init_scene();

#if KRG_HAS_EDITOR
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
    // Join all worker threads up front — the audio thread owns audio_system and the
    // render thread owns render state, both of which teardown below (and
    // glob_state_reset) destroy. Idempotent: run() already stopped them on the normal
    // path, and headless never started them.
    m_threads.stop();

    // Save session state to rtcache (non-headless only — headless tests don't touch session cfg)
    if (!m_headless)
    {
        glob::glob_state().getr_render().renderer.get_render_config().save();
        glob::glob_state().get_config()->save();
#if KRG_HAS_EDITOR
        ui::get_window<ui::bake_editor>()->save_config();
#endif

        glob::set_input_provider(nullptr);
    }

#if KRG_HAS_EDITOR
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

#if KRG_HAS_EDITOR
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

    // Release the content allocators' lane claims before the render system
    // (and its storages) goes away -- the storage dtor asserts no allocator
    // is still attached. Single-threaded here, so the direct form is legal.
    glob::glob_state().getr_render_translator().detach_content_storages();

    glob::glob_state().getr_render().loader.clear_caches();

    glob::glob_state().getr_render().renderer.deinit();

    glob::glob_state().getr_render().device.destruct();

    if (auto* phys = glob::glob_state().get_physics_system())
    {
        phys->shutdown();
    }

    // The physics thread is joined (stop() above), so no more results are produced.
    // Drain the last published-but-unconsumed snapshots so the ring is empty before
    // teardown. Results carry their transforms inline now, so there's nothing to free
    // — this just clears the ring (and reconciles the final snapshots if anything reads
    // them during shutdown).
    glob::glob_state().getr_physics_bridge().drain_results();

    // Release the static-collider allocator's lane claim on physics_system's BodyID
    // storage before that storage (owned by physics_system) is destroyed in
    // glob_state_reset -- the storage dtor asserts no allocator is still attached.
    // Single-threaded here (physics thread joined), so the direct form is legal.
    glob::glob_state().getr_physics_bridge().detach_storages();

    glob::glob_state_reset();
}

#if KRG_HAS_EDITOR
bool
vulkan_engine::wait_frames_rendered(int count, std::chrono::milliseconds timeout)
{
    // Thin delegate: the streaming handoff lives in engine_threads. The sole
    // caller, rpc waitFrame, already clamps untrusted input to [1,60].
    return m_threads.wait_frames_rendered(count, timeout);
}
#endif

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

    // Hand off render-state access to the render thread for the streaming phase:
    // main drops it here, the render thread grants itself in render_loop. From now
    // until stop(), any main-thread render mutation trips the assert.
    render::set_render_access(false);

    // Spawn the worker threads: the render thread (calls back into begin_frame on
    // main and draw_frame on itself) and the audio thread (from here audio_system is
    // owned by it; main only produces messages onto the audio channel). The audio /
    // physics loops drive the engine-owned processors handed in here.
    m_threads.start(*m_audio_processor, *m_physics_processor, *m_render_processor);

    // main loop
    for (;;)
    {
        // Check run duration limit
        if (m_run_for_seconds > 0.f && total_elapsed >= m_run_for_seconds)
        {
            ALOG_INFO("Run duration limit reached ({} seconds), exiting.", m_run_for_seconds);
            break;
        }
#if KRG_HAS_EDITOR
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

        // Pipeline gate + slot routing: block until the render thread has freed
        // the slot this frame will reuse (depth-1 — main builds frame N while
        // render draws N-1, never racing N-2's still-in-use slot), then route
        // this frame's camera/UI/command state into that parity slot. The vsync
        // / present-pacing stall the render thread takes each frame is exactly
        // the window this lets the main thread fill (the point of the decouple).
        m_threads.begin_frame();

#if KRG_HAS_EDITOR
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
#if KRG_HAS_IMGUI
        // Snapshot the just-built ImGui frame into this frame's render-owned slot.
        glob::glob_state().getr_render().renderer.capture_ui_snapshot();
#endif

        // Apply config edits the UI just made. apply_pending_render_config does
        // vkDeviceWaitIdle + swapchain/GPU-resource rebuilds and overwrites
        // m_render_config, which the render thread reads every frame — so it must
        // run with the render thread fully idle. Drain the whole pipeline first.
        // Config changes are rare (user toggles a setting), so the one-frame
        // stall is acceptable. When nothing is pending this is skipped entirely
        // (no write to m_render_config), keeping the common path fully decoupled.
        if (glob::glob_state().getr_render().renderer.has_pending_render_config())
        {
            m_threads.wait_idle();
            glob::glob_state().getr_render().renderer.apply_pending_render_config();
        }

        {
            KRG_make_scope(tick);
            KRG_PROFILE_SCOPE("Tick");
            tick(frame_time);
        }

        {
            auto& ctrs = ::kryga::glob::glob_state().getr_engine_counters();
            auto& vr = glob::glob_state().getr_render().renderer;

            // Render-thread-published stats — sampled via atomics so reading them
            // doesn't race the concurrent draw's counters / object cache.
            ctrs.all_draws.update(vr.stat_all_draws());
            ctrs.culled_draws.update(vr.stat_culled_draws());
            ctrs.objects.update(vr.stat_objects());
        }

        {
            KRG_make_scope(consume_updates);
            KRG_PROFILE_SCOPE("ConsumeUpdates");

            update_cameras();
            glob::glob_state().getr_render().renderer.set_camera(m_camera_data);

            consume_updated_physics();
            consume_updated_render();
            consume_updated_transforms();
        }

        // Publish the frame: its commands are all in the slot's queue; the render
        // thread drains that slot and draws. (No terminal command — one queue per
        // frame parity means draining to empty is the frame boundary.)
        m_threads.submit_frame();

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

    // Drain any in-flight frames and join the worker threads (render + audio).
    m_threads.stop();

    // Render thread gone — main reclaims access for single-threaded teardown,
    // and ownership of the pool guards with it (deinit frees system meshes/
    // materials/bindless slots and resets storages from here).
    render::set_render_access(true);
    glob::glob_state().getr_render().renderer.bind_render_pools_to_current_thread();
}
void
vulkan_engine::tick_headless()
{
    // No physics thread in headless — drive the engine-owned processor inline here.
    // Pump first so commands the previous frame's builder emitted get applied + stepped,
    // then drain the results into the snapshot before this frame's builder reads it. A
    // fixed nominal dt keeps headless deterministic (no wall clock to sample).
    m_physics_processor->pump(1.0f / 60.0f, /*paused=*/false);
    glob::glob_state().getr_physics_bridge().drain_results();

    // Process dirty-render items queued by level/package load
    consume_updated_physics();
    consume_updated_render();
    consume_updated_transforms();

    // Headless is single-threaded and never switches parity, so everything is
    // enqueued into and drained from slot 0.
    m_render_processor->drain(0);
    glob::glob_state().getr_render().renderer.draw_headless();
    glob::glob_state().getr_subsystem_queues().render.reset_arena();

    // No audio thread in headless — drain the audio channel synchronously here.
    consume_updated_audio();
}

#if KRG_HAS_EDITOR
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
#if KRG_HAS_EDITOR
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
    if (auto lvl = glob::glob_state().getr_model().current_level)
    {
        lvl->tick(dt);
    }
#endif

    if (auto* anim = glob::glob_state().get_animation_system())
    {
        anim->tick(dt);
    }

    // Audio runs on its own thread (engine_threads) draining the message channel and
    // ticking audio_system. Main only PRODUCES here: cancel voices whose emitter left
    // the model cache (deletion / unload / play->edit rollback) by emitting stop
    // intents. Runs every frame (outside the play gate) so rollback stops apply even
    // in edit mode. No audio_system access on the main thread.
    glob::glob_state().getr_audio_bridge().reap_orphans();

    // Physics also runs on its own thread (engine_threads::physics_loop). Main only
    // PRODUCES intents (via the destructible component / its render builder) and
    // CONSUMES results here: pull published chunk transforms + broken/expired state
    // into the per-handle snapshot the builder reads. Runs every frame (outside the
    // play gate) so the snapshot stays current even in edit mode.
    glob::glob_state().getr_physics_bridge().drain_results();
#if KRG_HAS_EDITOR
    // Freeze integration in edit mode; the worker still drains commands so transforms
    // and registrations stay synced for when play resumes.
    m_threads.set_physics_paused(!playing);
#endif

#if KRG_HAS_EDITOR
    if (!playing)
    {
        return;
    }
#endif
}

bool
vulkan_engine::load_level(const utils::id& level_id)
{
    auto& lm = glob::glob_state().getr_model().levels;

    // Tear down the current level if any.  Destroy commands are enqueued into
    // the SPSC queue (arena-allocated) and ride the current frame's build
    // frame slot; the render thread drains them as part of the next frame and
    // rewinds that frame slot only after drawing it (reset_frame_slot), so the commands'
    // memory survives until executed — no explicit arena retirement needed.
    // The render thread calls schedule_to_delete with authoritative
    // m_current_frame_number — no cross-thread read.
#if KRG_HAS_EDITOR
    glob::glob_state().getr_editor_system().editor.set_selected(utils::id());
#endif
    if (auto* prev = glob::glob_state().getr_model().current_level)
    {
        unload_render_resources(*prev);
        // Retire the prev level's lightmap binding on the render thread, ahead of
        // the new level's commands in this same frame slot's FIFO queue.
        auto* cmd =
            glob::glob_state().getr_subsystem_queues().render.alloc_cmd<destroy_lightmap_cmd>();
        cmd->level_id = prev->get_id();
        glob::glob_state().getr_subsystem_queues().render.enqueue(cmd);
        lm.unload_level(*prev);
    }

    auto result = lm.load_level(level_id);
    if (!result)
    {
        ALOG_FATAL("Nothing to do here!");
        return false;
    }

    glob::glob_state().getr_model().current_level = result;

    // Remember the current level as session state — persisted to rtcache on
    // shutdown so the editor reopens it next launch (committed default untouched).
    glob::glob_state().get_config()->level = level_id;

    // Create the lightmap atlas if the level references baked data. The texture
    // creation + bindless allocation are render-state ops, so they ride the render
    // command pipeline (create_lightmap_cmd) rather than running on the main thread:
    // load the manifest + raw atlas bytes here (pure model/disk work), flatten the
    // per-object UVs, and enqueue. The command executes on the render thread, in
    // FIFO order ahead of this level's object builds (enqueued later this frame in
    // consume_updated_render), which resolve the binding from the loader registry.
    if (result->has_lightmap_ref())
    {
        core::lightmap_manifest manifest;
        if (manifest.load(result->get_lightmap_manifest_rid()))
        {
            std::vector<uint8_t> lm_data;
            if (vfs::load_file(result->get_lightmap_bin_rid(), lm_data) && !lm_data.empty())
            {
                auto& iq = glob::glob_state().getr_subsystem_queues().render;
                auto* cmd = iq.alloc_cmd<create_lightmap_cmd>();
                cmd->level_id = result->get_id();
                cmd->tex_id = AID((result->get_id().str() + "_lightmap").c_str());
                cmd->width = manifest.atlas_width;
                cmd->height = manifest.atlas_height;
                cmd->pixels = std::move(lm_data);
                cmd->entries = render_convert::flatten_lightmap_manifest(manifest);
                iq.enqueue(cmd);
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
        auto& obj = *t.second;

        // Mirror container::unload(instance): a package object (readonly CDO) may
        // have leaked into the level cache via the load context. Its render data is
        // shared and not ours to free here — skip it and keep iterating.
        if (!obj.get_flags().instance_obj)
        {
            continue;
        }

        glob::glob_state().getr_render_translator().render_cmd_destroy(obj, true);
    }

    return true;
}

bool
vulkan_engine::unload_render_resources(core::package& l)
{
    auto& cs = l.get_local_cache();

    for (auto& t : cs.objects.get_items())
    {
        auto& obj = *t.second;

        // Mirror container::unload(package): a package must hold only package objects.
        KRG_check(!obj.get_flags().instance_obj,
                  "instance object found in package during render unload");

        glob::glob_state().getr_render_translator().render_cmd_destroy(obj, true);
    }

    return true;
}

void
vulkan_engine::consume_updated_transforms()
{
    auto& items = glob::glob_state().getr_model().dirty().dirty_transforms;

    if (items.empty())
    {
        return;
    }

    auto& rb = glob::glob_state().getr_render_translator();

    for (auto& i : items)
    {
        rb.render_cmd_transform(*i);
        i->set_dirty_transform(false);
    }

    items.clear();
}

namespace
{
// Emit the active camera's pose as a set_listener intent onto the audio channel.
// Producer-side only (model thread) — the audio thread applies it via the bridge.
// Same per-frame cadence as the old direct set_listener call.
void
emit_listener_pose(base::camera_component* cam)
{
    core::audio_message msg;
    msg.kind = core::audio_msg_kind::set_listener;
    msg.position = cam->get_owner()->get_position();
    msg.forward = cam->get_forward_vector().as_glm();
    msg.up = cam->get_up_vector().as_glm();
    glob::glob_state().getr_audio_bridge().emit(msg);
}
}  // namespace

void
vulkan_engine::update_cameras()
{
#if KRG_HAS_EDITOR
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

        emit_listener_pose(cam);
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

    emit_listener_pose(cam);
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
#if KRG_HAS_EDITOR
        // Editor-only debug helpers that populate the sandbox scene.
        glob::glob_state().getr_editor_system().editor.ev_spawn();
        glob::glob_state().getr_editor_system().editor.ev_lights();
#endif
    }

#if !KRG_HAS_EDITOR
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
    auto& mq = glob::glob_state().getr_model().dirty();
    auto& rb = glob::glob_state().getr_render_translator();

    for (auto& i : mq.destroy_render)
    {
        rb.render_cmd_destroy(*i, true);
    }
    mq.destroy_render.clear();
    mq.deferred_release.clear();

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

void
vulkan_engine::consume_updated_audio()
{
    // Headless / single-threaded path only: run() spawns the audio thread which is the
    // sole consumer, so this must NOT run there (it would be a second consumer on the
    // SPSC queue). In headless no thread exists, so the main thread does the audio
    // thread's work synchronously — drain the channel through the processor, tick the
    // renderer, reap orphans.
    auto& q = glob::glob_state().getr_subsystem_queues().audio;
    auto* as = glob::glob_state().get_audio_system();
    audio_message_processor& proc = *m_audio_processor;

    q.drain([&proc](core::audio_message msg) { proc.process(msg); });

    as->renderer.tick(0.f);
    glob::glob_state().getr_audio_bridge().reap_orphans();
}

void
vulkan_engine::consume_updated_physics()
{
    // Mirror of consume_updated_render for the physics world. Reuses the same model
    // dirty queues: a component with no physics handler is a no-op, so only physics
    // types (terrain, ...) produce commands. Runs every frame (independent of play
    // mode) so colliders exist by the time physics ticks.
    //
    // Ordering contract: this MUST run before consume_updated_render(), which clears
    // the queues — physics reads the same dirty list render does. We do NOT clear
    // here. The bridge only EMITS intents onto the model->physics ring; the physics
    // thread (physics_command_processor) drains and executes them against the Jolt
    // world. Nothing mutates the world on this thread — that would race the physics
    // thread's tick (the reason the old main-thread input_queue drain was removed).
    auto* ps = glob::glob_state().get_physics_system();
    if (!ps)
    {
        return;
    }

    auto& mq = glob::glob_state().getr_model().dirty();
    auto& pb = glob::glob_state().getr_physics_bridge();

    for (auto& i : mq.destroy_render)
    {
        pb.physics_cmd_destroy(*i, true);
    }

    for (auto& i : mq.dirty_render)
    {
        pb.physics_cmd_build(*i, true);
    }
}

}  // namespace kryga
