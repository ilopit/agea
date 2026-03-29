#pragma once

#include <vulkan_render/types/vulkan_gpu_types.h>
#include <vulkan_render/kryga_render.h>

#include <utils/id.h>
#include <utils/line_container.h>

#include <core/model_fwds.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
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
    render::render_mode render_mode = render::render_mode::instanced;
    float run_for_seconds = 0.f;  // 0 = unlimited
    bool show_help = false;

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

    // clang-format off
    void consume_updated_transforms();
    void consume_updated_render_components();
    void consume_updated_render_assets();
    void consume_updated_shader_effects();

    // clang-format on

    void
    render_thread_func();

    float m_run_for_seconds = 0.f;  // 0 = unlimited

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
