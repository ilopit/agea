#pragma once

#include <vulkan_render/types/vulkan_gpu_types.h>
#include <vulkan_render/kryga_render.h>

#include <resource_locator/resource_locator.h>

#include <utils/id.h>
#include <utils/line_container.h>
#include <utils/singleton_instance.h>

#include <core/model_fwds.h>

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
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
    vulkan_engine(std::unique_ptr<singleton_registry> r);
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

    std::unique_ptr<singleton_registry> m_registry;
    // clang-format on

    float m_run_for_seconds = 0.f;  // 0 = unlimited

    gpu::camera_data m_camera_data;

    glm::vec3 m_last_camera_position = glm::vec3{0.f};

    std::unique_ptr<sync_service> m_sync_service;
};

namespace glob
{
struct engine : public singleton_instance<::kryga::vulkan_engine, engine>
{
};
}  // namespace glob

}  // namespace kryga
