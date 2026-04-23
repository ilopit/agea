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
class sync_service;

// Command-line startup options
struct startup_options
{
    float run_for_seconds = 0.f;  // 0 = unlimited
    bool show_help = false;

    // Editor / headless integration (Phase 0).
    //
    // editor_ipc_name: when non-empty, the engine starts headless (no OS window,
    // no swapchain presentation) and will share frames over an IPC channel of
    // this name. The actual transport is wired in Phase 1. For Phase 0 the
    // name is accepted and logged.
    //
    // dump_first_frame_path: when non-empty, the engine runs headless, renders
    // a few frames to let uploads settle, dumps the color image to this path
    // as a PNG, then exits. Used to verify the headless pipeline end-to-end.
    //
    // headless_width / headless_height: resolution of the offscreen render
    // target used in headless mode.
    //
    // wait_for_debugger: prints the process PID and blocks until a debugger
    // attaches (or a 60s timeout fires as a safety).
    std::string editor_ipc_name;
    std::string dump_first_frame_path;
    uint32_t headless_width = 1024;
    uint32_t headless_height = 1024;
    bool wait_for_debugger = false;

    bool
    is_headless() const
    {
        return !editor_ipc_name.empty() || !dump_first_frame_path.empty();
    }

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

// Prints the current PID and blocks (up to `timeout`, default 60s) until a
// debugger attaches. On Linux, polls /proc/self/status:TracerPid; on Windows,
// polls IsDebuggerPresent(). Safe to call before init().
void
wait_for_debugger(std::chrono::seconds timeout = std::chrono::seconds(60));

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
    run_headless();
    void
    tick(float dt);

    void
    execute_sync_requests();

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

private:
    void
    update_cameras();

    bool
    load_level(const utils::id& level_id);

    void consume_updated_render();
    void consume_updated_transforms();

    void
    render_thread_func();

    float m_run_for_seconds = 0.f;  // 0 = unlimited

    // Headless / editor state — valid when m_headless is true.
    bool m_headless = false;
    std::string m_editor_ipc_name;
    std::string m_dump_first_frame_path;
    uint32_t m_headless_width = 1024;
    uint32_t m_headless_height = 1024;

    gpu::camera_data m_camera_data;

    glm::vec3 m_last_camera_position = glm::vec3{0.f};

    std::unique_ptr<sync_service> m_sync_service;

    // Render thread synchronization (lock-step)
    std::thread m_render_thread;
    std::mutex m_render_mutex;
    std::condition_variable m_render_cv;
    std::condition_variable m_main_cv;
    bool m_render_work_ready = false;
    bool m_render_done = true;
    bool m_render_shutdown = false;
};

}  // namespace kryga
