#pragma once

#include <vulkan_render/types/vulkan_gpu_types.h>

#include <resource_locator/resource_locator.h>

#include <utils/id.h>
#include <utils/line_conteiner.h>
#include <utils/singleton_instance.h>

#include <core/model_fwds.h>

#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

union SDL_Event;

namespace agea
{
class native_window;

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
    vulkan_engine(std::unique_ptr<singleton_registry> r);
    ~vulkan_engine();

    // initializes everything in the engine
    bool
    init();
    void
    cleanup();

    void
    run();
    void
    tick(float dt);

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

    render::gpu_camera_data m_camera_data;

    glm::vec3 m_last_camera_position = glm::vec3{0.f};
};

namespace glob
{
struct engine : public singleton_instance<::agea::vulkan_engine, engine>
{
};
}  // namespace glob

}  // namespace agea
