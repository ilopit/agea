#pragma once

#include <vulkan_render_types/vulkan_render_data.h>
#include <vulkan_render_types/vulkan_types.h>
#include <vulkan_render_types/vulkan_gpu_types.h>

#include <resource_locator/resource_locator.h>
#include <model_global_api/render_api.h>

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
class vulkan_loader;
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

class vulkan_engine : public ::agea::model_render_api
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
    singletone_autodeleter m_current_level;
    singletone_autodeleter m_class_objects_cache_set;
    singletone_autodeleter m_objects_cache_set;
    singletone_autodeleter m_package_manager;

    singletone_autodeleter m_window;
    singletone_autodeleter m_resource_locator;
    singletone_autodeleter m_render_loader;
    singletone_autodeleter m_render_device;
    singletone_autodeleter m_ui;
    // clang-format on

    render::gpu_scene_data m_scene_parameters;

    player_camera m_camera;

    int m_frame_number{0};

    std::unordered_map<int, bool> m_pressed;

    rendering_queues m_qs;
    bool m_force_shader_recompile = false;

    virtual void
    add_to_render_queue(model::renderable* r)
    {
        m_qs.add_to_queue(r);
    }

    virtual void
    invalidate(model::renderable* r)
    {
        m_qs.add_to_dirty_queue(r);
    }

    virtual void
    remove_from_render_queue(model::renderable* r)
    {
        m_qs.remove_from_rdc(r);
    }
};

namespace glob
{
struct engine : public selfcleanable_singleton<::agea::vulkan_engine>
{
};
}  // namespace glob

}  // namespace agea
