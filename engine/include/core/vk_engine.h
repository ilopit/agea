#pragma once

#include "vulkan_render/vulkan_types.h"

#include "fs_locator.h"

#include "utils/weird_singletone.h"
#include "rendering queues.h"

#include "model/model_fwds.h"

#include <algorithm>
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

namespace render
{
class loader;
class render_device;
struct frame_data;
struct shader_effect;
}  // namespace render

struct player_camera
{
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 inputAxis;

    float pitch{0};  // up-down rotation
    float yaw{0};    // left-right rotation

    glm::mat4
    get_rotation_matrix();
};

class vulkan_engine
{
public:
    vulkan_engine();
    ~vulkan_engine();

    // initializes everything in the engine
    bool
    init();
    void
    cleanup();
    void
    handle_dirty_objects();
    void
    draw();
    void
    run();

    render::frame_data&
    get_current_frame();

    void
    draw_new_objects(VkCommandBuffer cmd);

    rendering_queues&
    qs()
    {
        return m_qs;
    }

private:
    void
    process_input_event(SDL_Event* ev);

    void
    update_camera(float deltaSeconds);

    void
    init_scene();

    void
    event_reload_world();

    void
    compile_all_shaders();

    // clang-format off
    std::unique_ptr<closure<model::level>>               m_current_level;
    std::unique_ptr<closure<model::cache_set>>           m_class_objects_cache_set;
    std::unique_ptr<closure<model::cache_set>>           m_objects_cache_set;
    std::unique_ptr<closure<model::package_manager>>     m_package_manager;

    std::unique_ptr<closure<native_window>>              m_window;
    std::unique_ptr<closure<resource_locator>>           m_resource_locator;
    std::unique_ptr<closure<render::loader>>             m_render_loader;
    std::unique_ptr<closure<render::render_device>>      m_render_device;
    std::unique_ptr<closure<ui::ui>>                     m_ui;
    std::unique_ptr<closure<editor::cli>>                m_editor_cli;
    // clang-format on

    render::gpu_scene_data m_scene_parameters;

    player_camera m_camera;

    int m_frame_number{0};

    std::unordered_map<int, bool> m_pressed;

    rendering_queues m_qs;
    bool m_force_shader_recompile = false;
};

namespace glob
{
struct engine : public weird_singleton<::agea::vulkan_engine>
{
};
}  // namespace glob

}  // namespace agea
