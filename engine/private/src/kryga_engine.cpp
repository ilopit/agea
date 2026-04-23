#include "engine/kryga_engine.h"

#include "engine/ui.h"
#include "engine/input_manager.h"
#include "engine/editor.h"
#include "engine/config.h"
#include "engine/engine_counters.h"
#include "engine/private/ui/bake_editor.h"
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

#include <core/queues.h>
#include <render_bridge/render_bridge.h>
#include <render_bridge/render_commands_common.h>

#include <vfs/vfs.h>

#include <vulkan_render/kryga_render.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/types/vulkan_mesh_data.h>
#include <vulkan_render/types/vulkan_texture_data.h>
#include <vulkan_render/types/vulkan_render_pass.h>
#include <vulkan_render/utils/readback.h>
#include <vulkan_render/vk_descriptors.h>
#include <core/lightmap_manifest.h>

#include <render/utils/image_compare.h>

#include <editor_ipc/frame_protocol.h>
#include <editor_ipc/frame_publisher.h>

#include <vfs/io.h>

#include <animation/animation_system.h>

#include <utils/kryga_log.h>
#include <utils/process.h>
#include <utils/clock.h>

#include <sol2_unofficial/sol.h>

#include <CLI/CLI.hpp>

#include <fstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <string_view>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#endif

namespace kryga
{

// ============================================================================
// Startup Options
// ============================================================================

namespace
{
bool
is_debugger_present_now()
{
#if defined(_WIN32)
    return ::IsDebuggerPresent() != 0;
#elif defined(__linux__)
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line))
    {
        constexpr std::string_view key = "TracerPid:";
        if (line.rfind(key, 0) == 0)
        {
            int pid = std::atoi(line.data() + key.size());
            return pid != 0;
        }
    }
    return false;
#else
    return false;
#endif
}

void
add_common_options(CLI::App& app, startup_options& out)
{
    app.add_option("-t,--run-for", out.run_for_seconds,
                   "Run for specified duration then exit (0 = unlimited)")
        ->check(CLI::NonNegativeNumber);

    app.add_option("--editor-ipc", out.editor_ipc_name,
                   "Run headless and publish frames on this IPC channel "
                   "(Phase 0: name is accepted and logged; transport wired in Phase 1).");

    app.add_option("--dump-first-frame", out.dump_first_frame_path,
                   "Run headless, render a warm-up burst, then save the final color "
                   "image to this PNG path and exit. Used to verify the headless pipeline.");

    app.add_option("--headless-width", out.headless_width,
                   "Headless render target width (default 1024).")
        ->check(CLI::PositiveNumber);
    app.add_option("--headless-height", out.headless_height,
                   "Headless render target height (default 1024).")
        ->check(CLI::PositiveNumber);

    app.add_flag("--wait-for-debugger", out.wait_for_debugger,
                 "Print the process PID and block until a debugger attaches.");
}
}  // namespace

bool
startup_options::parse(int argc, char** argv, startup_options& out)
{
    out = {};

    CLI::App app{"Kryga Engine"};
    add_common_options(app, out);

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
    startup_options dummy;
    add_common_options(app, dummy);

    ALOG_INFO("{}", app.help());
}

void
wait_for_debugger(std::chrono::seconds timeout)
{
#if defined(_WIN32)
    auto pid = static_cast<unsigned long>(::GetCurrentProcessId());
#else
    auto pid = static_cast<unsigned long>(::getpid());
#endif

    ALOG_INFO("Waiting for debugger to attach. PID={} (timeout {}s)",
              pid,
              static_cast<long long>(timeout.count()));

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (is_debugger_present_now())
        {
            ALOG_INFO("Debugger attached — continuing.");
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    ALOG_WARN("wait_for_debugger: timeout elapsed, continuing without debugger.");
}

vulkan_engine::vulkan_engine()
    : m_sync_service(std::make_unique<sync_service>())
    , m_frame_publisher(std::make_unique<editor_ipc::frame_publisher>())
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

    m_headless = options.is_headless();
    m_editor_ipc_name = options.editor_ipc_name;
    m_dump_first_frame_path = options.dump_first_frame_path;
    m_headless_width = options.headless_width;
    m_headless_height = options.headless_height;
    if (m_headless)
    {
        ALOG_INFO("Headless mode: {}x{}, editor-ipc='{}', dump-first-frame='{}'",
                  m_headless_width,
                  m_headless_height,
                  m_editor_ipc_name,
                  m_dump_first_frame_path);
    }

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
    state_mutator__game_editor::set(gs);
    state_mutator__input_manager::set(gs);
    glob::set_input_provider(glob::glob_state().get_input_manager());
    state_mutator__render_device::set(gs);
    state_mutator__vulkan_render::set(gs);
    state_mutator__vulkan_render_loader::set(gs);
    state_mutator__ui::set(gs);
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

    auto rp_main = glob::glob_state().getr_vfs().real_path(vfs::rid("data://configs/kryga.acfg"));
    glob::glob_state().get_config()->load(APATH(rp_main.value()));

    auto rp_input = glob::glob_state().getr_vfs().real_path(vfs::rid("data://configs/inputs.acfg"));
    glob::glob_state().get_input_manager()->load_actions(APATH(rp_input.value()));

    // Load render config (with rtcache fallback for session state)
    render::render_config render_cfg;
    render_cfg.load_with_cache(vfs::rid("data://configs/render.acfg"),
                               vfs::rid("rtcache://render.acfg"));

    glob::glob_state().get_game_editor()->init();

    gs.schedule_action(gs::state::state_stage::init,
                       [](kryga::gs::state& s) { s.get_pm()->init(); });
    gs.run_init();

    native_window::construct_params rwc;
    rwc.w = 1600 * 2;
    rwc.h = 900 * 2;
    auto window = glob::glob_state().get_native_window();

    if (m_headless)
    {
        // No OS window / SDL init — but still report a valid size to every
        // subsystem that queries native_window for aspect ratio, viewport
        // extent, etc. Override rwc with the headless resolution so the
        // render target and cluster grid are sized correctly.
        rwc.w = static_cast<int>(m_headless_width);
        rwc.h = static_cast<int>(m_headless_height);
        window->set_headless_size(rwc.w, rwc.h);
    }
    else
    {
        if (!window->construct(rwc))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    render::render_device::construct_params rdc;
    rdc.window = m_headless ? nullptr : window->handle();
    rdc.headless = m_headless;

    auto device = glob::glob_state().get_render_device();
    if (!device->construct(rdc))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    // ImGui + editor UI windows need a real SDL window + swapchain. In headless
    // mode we drive the scene programmatically (or via IPC in later phases).
    if (!m_headless)
    {
        glob::glob_state().getr_ui().init();

        // Load bake config (with rtcache fallback for session state)
        ui::get_window<ui::bake_editor>()->init(vfs::rid("data://configs/bake.acfg"),
                                                vfs::rid("rtcache://bake.acfg"));
    }

    glob::glob_state().getr_vulkan_render().init(rwc.w, rwc.h, render_cfg);

    init_default_resources();

    init_scene();

    m_sync_service->start();

    if (m_headless && !m_editor_ipc_name.empty())
    {
        editor_ipc::frame_publisher::config pub_cfg;
        pub_cfg.name = m_editor_ipc_name;
        pub_cfg.max_width = m_headless_width;
        pub_cfg.max_height = m_headless_height;
        pub_cfg.format = editor_ipc::pf_rgba8;

        if (!m_frame_publisher->init(*device, pub_cfg))
        {
            ALOG_ERROR("frame_publisher init failed: {}", m_frame_publisher->last_error());
            return false;
        }
    }

    ALOG_INFO("Initialization completed");
    return true;
}

void
vulkan_engine::cleanup()
{
    // In headless we never created the UI / bake editor, so skip the state
    // save that touches them.
    if (!m_headless)
    {
        glob::glob_state().getr_vulkan_render().get_render_config().save_to_cache(
            vfs::rid("rtcache://render.acfg"));
        ui::get_window<ui::bake_editor>()->save_config();
    }

    glob::set_input_provider(nullptr);

    m_sync_service->stop();

    if (m_frame_publisher && m_frame_publisher->is_open())
    {
        m_frame_publisher->shutdown(*glob::glob_state().get_render_device());
    }

    glob::glob_state().get_render_device()->wait_for_fences();

    glob::glob_state().getr_vulkan_render().deinit();

    glob::glob_state().get_vulkan_render_loader()->clear_caches();

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
        // Wait for previous frame's render to finish before touching render state.
        // First iteration is a no-op (m_render_done starts true).
        {
            std::unique_lock lock(m_render_mutex);
            m_main_cv.wait(lock, [this] { return m_render_done; });
        }

        // Render thread is done — all commands executed and destructed.
        // Safe to reclaim arena memory for next frame.
        glob::glob_state().getr_render_bridge().reset_arena();

        {
            auto& ctrs = ::kryga::glob::glob_state().getr_engine_counters();
            auto& vr = glob::glob_state().getr_vulkan_render();

            ctrs.all_draws.update(vr.get_all_draws());
            ctrs.culled_draws.update(vr.get_culled_draws());
            ctrs.objects.update(vr.get_cache().objects.get_actual_size());
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
vulkan_engine::run_headless()
{
    KRG_check(m_headless, "run_headless requires headless mode");

    auto& device = glob::glob_state().getr_render_device();
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& bridge = glob::glob_state().getr_render_bridge();

    // The render graph and async uploads take a couple of frames to become
    // coherent (FRAMES_IN_FLIGHT = 3). Render a small warm-up burst before
    // dumping so the dumped frame matches what a steady-state viewer sees.
    const uint32_t warmup_frames = static_cast<uint32_t>(FRAMES_IN_FLIGHT) + 1;

    const float frame_time = 1.f / glob::glob_state().get_config()->fps_lock;
    float total_elapsed = 0.f;
    uint32_t frames_drawn = 0;

    ALOG_INFO("Headless loop: starting (warmup={} frames)", warmup_frames);

    while (true)
    {
        if (m_run_for_seconds > 0.f && total_elapsed >= m_run_for_seconds)
        {
            ALOG_INFO("Run duration limit reached ({} seconds), exiting.", m_run_for_seconds);
            break;
        }

        KRG_make_scope(frame);
        KRG_PROFILE_SCOPE("HeadlessFrame");

        // Order matches run(): arena reclaim first (previous frame is done,
        // this is the only thread that consumes render commands), then tick /
        // enqueue, then drain + draw.
        bridge.reset_arena();

        // Phase 4: drain control messages. See handle_ipc_message for
        // the message format. Phase 5: resend schemas whenever a
        // consumer attaches (0→1 transition) so a late-connecting editor
        // always has them.
        if (m_frame_publisher && m_frame_publisher->is_open())
        {
            const bool attached_now = m_frame_publisher->is_consumer_attached();
            if (attached_now && !m_schemas_sent_for_consumer)
            {
                send_schemas();
                m_schemas_sent_for_consumer = true;
            }
            else if (!attached_now && m_schemas_sent_for_consumer)
            {
                m_schemas_sent_for_consumer = false;
            }

            m_frame_publisher->drain_messages_in(
                [this](std::string_view msg) { handle_ipc_message(msg); });
        }

        // Phase 2: drain input events from the VS Code side and route
        // them into the edit-mode camera. Mouse-move deltas become
        // look-up / look-left; mouse-button[R] toggles "looking" which
        // is the same gate the SDL path uses for right-drag camera.
        if (m_frame_publisher && m_frame_publisher->is_open())
        {
            struct accumulator
            {
                float look_up = 0.f;
                float look_left = 0.f;
                bool looking = false;
                bool looking_set = false;
            } acc;
            constexpr float mouse_sensitivity = 0.1f;
            m_frame_publisher->drain_input(
                [&](const editor_ipc::input_event& e)
                {
                    switch (e.type)
                    {
                    case editor_ipc::iev_mouse_move:
                        acc.look_up -= static_cast<float>(e.d) * mouse_sensitivity;
                        acc.look_left -= static_cast<float>(e.c) * mouse_sensitivity;
                        break;
                    case editor_ipc::iev_mouse_button:
                        if (e.a == 1)  // right button
                        {
                            acc.looking = (e.b != 0);
                            acc.looking_set = true;
                        }
                        break;
                    default:
                        break;
                    }
                });
            if (acc.look_up != 0.f || acc.look_left != 0.f || acc.looking_set)
            {
                auto* editor = glob::glob_state().get_game_editor();
                editor->apply_ipc_input(
                    0.f, 0.f, acc.look_up, acc.look_left,
                    acc.looking_set ? acc.looking : acc.look_up != 0.f || acc.look_left != 0.f);
            }
        }

        tick(frame_time);
        execute_sync_requests();

        update_cameras();
        renderer.set_camera(m_camera_data);

        consume_updated_render();
        consume_updated_transforms();

        bridge.drain_queue();
        renderer.draw_headless();

        ++frames_drawn;

        const bool warm = frames_drawn >= warmup_frames;

        // Source of truth for the rendered frame: the "main" render pass'
        // current-frame color image. draw_headless() leaves it in
        // COLOR_ATTACHMENT_OPTIMAL; both the publisher and the PNG path
        // barrier it through TRANSFER_SRC_OPTIMAL and back.
        auto* main_pass = renderer.get_render_pass(AID("main"));
        KRG_check(main_pass, "main render pass must exist to publish / dump frame");
        const auto& color_images = main_pass->get_color_images();
        KRG_check(!color_images.empty(), "main render pass has no color images");
        const auto frame_idx = device.get_current_frame_index();
        auto* color_image = color_images[frame_idx % color_images.size()].get();
        KRG_check(color_image, "main render pass color image slot is null");

        if (warm && m_frame_publisher && m_frame_publisher->is_open())
        {
            m_frame_publisher->publish(device,
                                       color_image->image(),
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       main_pass->get_color_format(),
                                       m_headless_width,
                                       m_headless_height);
        }

        if (warm && !m_dump_first_frame_path.empty())
        {
            vkDeviceWaitIdle(device.vk_device());

            auto pixels = render::readback_framebuffer(
                *main_pass, m_headless_width, m_headless_height);

            if (render::save_png(m_dump_first_frame_path,
                                 pixels.data(),
                                 m_headless_width,
                                 m_headless_height))
            {
                ALOG_INFO("Headless: dumped first frame to '{}' ({}x{})",
                          m_dump_first_frame_path,
                          m_headless_width,
                          m_headless_height);
            }
            else
            {
                ALOG_ERROR("Headless: failed to save PNG to '{}'", m_dump_first_frame_path);
            }

            if (m_editor_ipc_name.empty())
            {
                break;
            }

            // Dump once per run.
            m_dump_first_frame_path.clear();
        }

        total_elapsed += frame_time;
        KRG_PROFILE_FRAME_MARK();
    }

    ALOG_INFO("Headless loop: drew {} frame(s)", frames_drawn);
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
    // plane_mesh is now created by vulkan_render::prepare_system_resources()
}

void
vulkan_engine::init_scene()
{
    auto level_id = AID("light_sandbox_baked");
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
                60.f,
                glob::glob_state().getr_native_window().aspect_ratio(),
                (float)KGPU_znear,
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
        "pm",
        sol::no_constructor,
        "get_package",
        [pm](const char* id) -> core::package* { return pm->get_package(AID(id)); },
        "load",
        [pm](const char* id) -> bool { return pm->load_package(AID(id)); });

    gs.create_box_with_obj("lua_pm", std::move(lua_pm));
}

namespace
{
// Split an ASCII message into tokens on whitespace. Each token that
// contains '=' is treated as key=value; tokens without '=' count as flags
// stored in `kv[token] = ""`. The caller ideally puts the message verb
// first (also stored in `kv["_"] = verb`).
struct parsed_msg
{
    std::unordered_map<std::string, std::string> kv;
};

parsed_msg
parse_ipc_msg(std::string_view msg)
{
    parsed_msg out;
    size_t i = 0;
    bool first = true;
    while (i < msg.size())
    {
        while (i < msg.size() && std::isspace(static_cast<unsigned char>(msg[i]))) ++i;
        size_t start = i;
        while (i < msg.size() && !std::isspace(static_cast<unsigned char>(msg[i]))) ++i;
        if (start == i) break;
        std::string_view tok(msg.data() + start, i - start);

        if (first)
        {
            out.kv["_"] = std::string(tok);
            first = false;
            continue;
        }
        auto eq = tok.find('=');
        if (eq == std::string_view::npos)
        {
            out.kv[std::string(tok)] = "";
        }
        else
        {
            out.kv[std::string(tok.substr(0, eq))] = std::string(tok.substr(eq + 1));
        }
    }
    return out;
}

float
to_float(const std::string& s, float fallback)
{
    try
    {
        return std::stof(s);
    }
    catch (...)
    {
        return fallback;
    }
}
}  // namespace

void
vulkan_engine::send_schemas()
{
    if (!m_frame_publisher || !m_frame_publisher->is_open()) return;

    // Phase 5: walk the reflection registry and emit one `schema` record
    // per registered type. Record format:
    //
    //     schema type=<name> [f=<field_name>:<type_id>:<offset> ...]
    //
    // The consumer caches these by type name. Records longer than
    // MSG_SLOT_BYTES are truncated by frame_publisher::push_message_out;
    // that's acceptable for Phase 5 (types with >50 fields are rare and
    // the truncated record still teaches the consumer the first N).
    // Chunking can be added if this becomes a real problem.
    auto* reg = glob::glob_state().get_rm();
    if (!reg) return;
    const auto& types = reg->get_types_to_name();

    uint32_t sent = 0;
    for (const auto& [type_name, rtype] : types)
    {
        if (!rtype) continue;

        std::string line = "schema type=";
        line += rtype->type_name.str();

        for (const auto& prop : rtype->m_properties)
        {
            if (!prop) continue;
            char buf[96];
            const int n = std::snprintf(buf, sizeof(buf), " f=%s:%d:%zu",
                                        prop->name.c_str(),
                                        prop->type.type_id,
                                        prop->offset);
            if (n > 0) line.append(buf, static_cast<size_t>(n));
        }

        if (m_frame_publisher->push_message_out(line)) ++sent;
    }

    ALOG_INFO("send_schemas: pushed {} type schemas", sent);
}

void
vulkan_engine::send_selection()
{
    if (!m_frame_publisher || !m_frame_publisher->is_open()) return;

    // Phase 4 exposes a single hardcoded selection: the editor's camera
    // position. Phase 5 generalises this via the reflection system.
    auto* editor = glob::glob_state().get_game_editor();
    const glm::vec3 pos{editor->get_camera_data().position};

    char buf[256];
    const int n = std::snprintf(
        buf, sizeof(buf),
        "selection entity=play_camera pos_x=%.4f pos_y=%.4f pos_z=%.4f",
        pos.x, pos.y, pos.z);
    if (n > 0)
    {
        m_frame_publisher->push_message_out(
            std::string_view(buf, static_cast<size_t>(n)));
    }
}

void
vulkan_engine::handle_ipc_message(std::string_view msg)
{
    if (msg.empty()) return;
    auto parsed = parse_ipc_msg(msg);
    const auto& verb = parsed.kv["_"];

    if (verb == "requestSelection")
    {
        send_selection();
        return;
    }
    if (verb == "setProperty")
    {
        const auto& path = parsed.kv["path"];
        const auto& value_s = parsed.kv["value"];
        const float value = to_float(value_s, 0.f);

        // Phase 4: only camera.position.{x,y,z} is wired. Extending to
        // arbitrary objects / components is Phase 5's job.
        auto* editor = glob::glob_state().get_game_editor();
        auto data = editor->get_camera_data();
        glm::vec3 pos{data.position};
        if (path == "camera.position.x") pos.x = value;
        else if (path == "camera.position.y") pos.y = value;
        else if (path == "camera.position.z") pos.z = value;
        else
        {
            ALOG_WARN("handle_ipc_message: unknown property path '{}'", path);
            return;
        }

        // Reach into the editor's state via an explicit setter rather than
        // poking private fields. Avoids an unrelated header-rotation
        // churn; the new method is tiny.
        editor->set_camera_position(pos);

        char echo[128];
        const int n = std::snprintf(echo, sizeof(echo),
                                    "propertyChanged path=%s value=%.6f",
                                    path.c_str(), value);
        if (n > 0 && m_frame_publisher)
        {
            m_frame_publisher->push_message_out(
                std::string_view(echo, static_cast<size_t>(n)));
        }
        return;
    }

    ALOG_WARN("handle_ipc_message: unknown verb '{}'", verb);
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
