#pragma once

#include <vulkan_render/types/vulkan_gpu_types.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/render_config.h>

#include <utils/id.h>
#include <utils/line_container.h>

#include <core/model_fwds.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>

union SDL_Event;

namespace kryga
{
class native_window;
namespace ui
{
struct editor_console;
}
#if KRG_EDITOR
namespace rpc
{
class rpc_server;
class rpc_log_sink;
}  // namespace rpc
#endif

// Command-line startup options
struct startup_options
{
    float run_for_seconds = 0.f;  // 0 = unlimited
    std::string level;            // empty = default (light_sandbox_baked)
    std::string discovery;        // empty = default (tmp/editor_rpc.json)
    bool show_help = false;

    // Test-harness / headless mode (set programmatically, not parsed from CLI).
    // Skips SDL window, input manager, UI, game_editor. Uses offscreen render target.
    // Pair with tick_headless() for frame rendering.
    bool headless = false;

    // Parse command line arguments
    // Returns false if parsing failed or help was requested
    static bool
    parse(int argc, char** argv, startup_options& out);

    static void
    print_help(const char* program_name);
};

namespace ui
{
class ui;
}

namespace editor
{
class cli;
}

class vulkan_engine
{
public:
    vulkan_engine();
    ~vulkan_engine();

    // initializes everything in the engine
    bool
    init(const startup_options& options = {});
    void
    cleanup();

    void
    run();
    void
    tick(float dt);

    // Run one render frame without input/UI/event pump. Used in headless test mode.
    // Caller is responsible for any prior camera/render_bridge setup.
    void
    tick_headless();

    void
    init_default_resources();

    void
    init_scene();

    void
    init_default_scripting();

    bool
    unload_render_resources(core::level& l);

    bool
    unload_render_resources(core::package& l);

    // Load a level by id: runs the level_manager pipeline, marks it current,
    // uploads lightmap texture if baked. Public so headless tests can drive it.
    bool
    load_level(const utils::id& level_id);

#if KRG_EDITOR
    // RPC handlers run on the server's I/O thread. ALL state access goes
    // through this queue so the main thread is the sole owner of engine
    // state — no mutexes scattered across mutated fields.
    //
    // queue_main_action: fire-and-forget. Use for high-frequency mutations
    //   where I/O-thread latency matters more than per-call confirmation
    //   (e.g. properties.set during drag-scrub).
    // wait_main_action:  queue + block on completion. Use when the response
    //   needs to reflect the work's result (reads, single mutations that
    //   want explicit ok/err in the response).
    void
    queue_main_action(std::function<void()> a);
    bool
    wait_main_action(std::function<void()> a,
                     std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // Block (on the caller's thread, e.g. an RPC I/O thread) until the render
    // thread has finished drawing `count` more frames than were in flight at the
    // call. Because the streaming pipeline lets the main thread build the next
    // frame while the render thread is still drawing the current one, simply
    // round-tripping a main action no longer guarantees a model mutation has
    // propagated to the render cache — the command only executes when the render
    // thread drains that frame. RPC waitFrame uses this so a following render.*
    // query observes the mutation. Returns false on timeout.
    bool
    wait_frames_rendered(int count,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    void
    request_shutdown()
    {
        m_shutdown_requested.store(true, std::memory_order_relaxed);
    }

    rpc::rpc_server&
    get_rpc_server()
    {
        return *m_rpc_server;
    }
#endif

private:
    void
    update_cameras();

    void
    consume_updated_render();
    void
    consume_updated_transforms();

    void
    rebuild_physics_static_world();

    void
    render_thread_func();

    // Select the per-frame double-buffer slot (frame parity) for the frame about
    // to be built, across BOTH producers of slot-indexed state: the renderer
    // (camera + UI snapshot) and the command queues/arena. They live in separate
    // subsystems but must always name the same slot — this is the single point
    // that keeps them in sync.
    void
    select_frame_slot(uint32_t slot);

    float m_run_for_seconds = 0.f;  // 0 = unlimited
    std::string m_initial_level;
    std::string m_discovery_path;
    bool m_headless = false;

    gpu::camera_data m_camera_data;

    glm::vec3 m_last_camera_position = glm::vec3{0.f};

#if KRG_HAS_IMGUI
    std::unique_ptr<ui::editor_console> m_console;
#endif

#if KRG_EDITOR
    std::unique_ptr<rpc::rpc_server> m_rpc_server;
    std::shared_ptr<rpc::rpc_log_sink> m_rpc_log_sink;
    // Polled each tick to detect spawn/destroy without instrumenting core.
    // Count-only signal: misses same-count membership swaps, good enough for
    // Phase B tree refresh.
    std::atomic<bool> m_shutdown_requested{false};
    size_t m_last_known_object_count = 0;

    // Track editor/play mode transitions so external tooling sees changes
    // even when triggered via the engine's own UI (F5/Esc).
    int m_last_known_mode = -1;
    utils::id m_last_known_selection;

    // Backing storage + drain helper for the public queue_main_action /
    // wait_main_action above.
    std::mutex m_rpc_action_mutex;
    std::vector<std::function<void()>> m_rpc_actions;
    void
    drain_main_actions();
#endif

    // Render thread synchronization (streaming, depth-1 pipeline).
    // m_frames_submitted: frames the main thread has fully enqueued into the
    //   frame's parity slot queue. Bumped by main under m_render_mutex.
    // m_frames_completed: frames the render thread has drawn. Bumped by render
    //   under m_render_mutex.
    // Invariant: m_frames_submitted - m_frames_completed <= 1 — main builds
    // frame N (into arena/camera/UI slot N&1) while render draws N-1 (slot
    // (N-1)&1). m_render_cv wakes the render thread when a frame is available;
    // m_main_cv wakes the main thread when a frame completes (gate) or, with
    // has_pending_render_config, when the pipeline has fully drained to idle.
    std::thread m_render_thread;
    std::mutex m_render_mutex;
    std::condition_variable m_render_cv;
    std::condition_variable m_main_cv;
    uint64_t m_frames_submitted = 0;
    uint64_t m_frames_completed = 0;
    bool m_render_shutdown = false;
};

}  // namespace kryga
