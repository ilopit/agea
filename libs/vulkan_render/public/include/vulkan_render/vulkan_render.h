#pragma once

#include "vulkan_render/types/vulkan_render_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_light_data.h"
#include "vulkan_render/utils/vulkan_buffer.h"
#include "vulkan_render/utils/vulkan_image.h"
#include "vulkan_render/utils/segments.h"

#include <resource_locator/resource_locator.h>

#include <utils/singleton_instance.h>
#include <utils/id.h>
#include <utils/line_conteiner.h>
#include <utils/id_allocator.h>

#include <algorithm>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

namespace agea
{
namespace render
{

class vulkan_render_loader;
class render_device;
struct frame_data;
class shader_effect_data;

using render_line_conteiner = ::agea::utils::line_conteiner<render::object_data*>;

using materials_update_queue = ::agea::utils::line_conteiner<render::material_data*>;
using materials_update_queue_set = ::agea::utils::line_conteiner<materials_update_queue>;
using objects_update_queue = ::agea::utils::line_conteiner<render::object_data*>;
using lights_update_queue = ::agea::utils::line_conteiner<render::light_data*>;

struct frame_state
{
    bool
    has_obj_data() const
    {
        return !m_objects_queue.empty();
    }

    void
    reset_obj_data()
    {
        m_objects_queue.clear();
    }

    bool
    has_light_data() const
    {
        return !m_lights_queue.empty();
    }

    void
    reset_light_data()
    {
        m_lights_queue.clear();
    }

    bool
    has_mat_data() const
    {
        return has_materials;
    }

    void
    reset_mat_data()
    {
        has_materials = false;
    }

    void
    clear_upload_queues()
    {
        m_objects_queue.clear();
        m_lights_queue.clear();

        for (auto& m : m_materias_queue_set)
        {
            m.clear();
        }
    }

    vk_utils::vulkan_buffer m_dynamic_data_buffer;
    vk_utils::vulkan_buffer m_object_buffer;
    vk_utils::vulkan_buffer m_lights_buffer;
    vk_utils::vulkan_buffer m_materials_buffer;

    vk_utils::vulkan_buffer m_ui_vertex_buffer;
    vk_utils::vulkan_buffer m_ui_index_buffer;

    int32_t m_ui_vertex_count = 0;
    int32_t m_ui_index_count = 0;

    objects_update_queue m_objects_queue;
    materials_update_queue_set m_materias_queue_set;
    lights_update_queue m_lights_queue;

    bool has_materials = false;
    bool has_lights = false;
    frame_data* frame = nullptr;
};

class vulkan_render
{
public:
    vulkan_render();
    ~vulkan_render();

    void
    init();

    void
    deinit();

    void
    set_camera(render::gpu_camera_data d);

    void
    draw_objects();

    void
    add_object(render::object_data* obj_data);

    void
    drop_object(render::object_data* obj_data);

    void
    add_material(render::material_data* obj_data);

    void
    drop_material(render::material_data* obj_data);

    void
    schedule_material_data_gpu_upload(render::material_data* md);

    void
    schedule_game_data_gpu_upload(render::object_data* od);

    void
    schedule_light_data_gpu_upload(render::light_data* ld);

    void
    clear_upload_queue();

    void
    collect_lights();

private:
    void
    draw_objects(render::frame_state& frame);

    void
    build_global_set(render::frame_state& current_frame);

    void
    build_light_set(render::frame_state& current_frame);

    void
    upload_obj_data(render::frame_state& frame);

    void
    upload_light_data(render::frame_state& frame);

    void
    upload_material_data(render::frame_state& frame);

    void
    draw_outline_objects_queue(render_line_conteiner& r,
                               VkCommandBuffer cmd,
                               vk_utils::vulkan_buffer& obj_tb,
                               vk_utils::vulkan_buffer& dyn_tb,
                               render::frame_state& current_frame);

    void
    draw_objects_queue(render_line_conteiner& r,
                       VkCommandBuffer cmd,
                       vk_utils::vulkan_buffer& obj_tb,
                       vk_utils::vulkan_buffer& dyn_tb,
                       render::frame_state& current_frame);

    void
    push_config(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout, uint32_t mat_id);

    void
    upload_gpu_object_data(render::gpu_object_data* object_SSBO);

    void
    upload_gpu_materials_data(uint8_t* object_SSBO, materials_update_queue& queue);

    void
    update_transparent_objects_queue();

    frame_state&
    get_current_frame_transfer_data();

public:
    void
    prepare_system_resources();
    void
    prepare_ui_pipeline();
    void
    prepare_ui_resources();

    void
    update_ui(frame_state& cmd);
    void
    draw_ui(frame_state& cmd);
    void
    resize(uint32_t width, uint32_t height);

    render::gpu_scene_data m_scene_parameters;
    render::gpu_camera_data m_camera_data;

    utils::id_allocator m_dir_lights_idalloc;
    std::vector<render::gpu_directional_light_data> m_dir_lights;

    utils::id_allocator m_spot_lights_idalloc;
    std::vector<render::gpu_spot_light_data> m_spot_lights;

    utils::id_allocator m_point_lights_idalloc;
    std::vector<render::gpu_point_light_data> m_point_lights;

    glm::vec3 m_last_camera_position = glm::vec3{0.f};

    std::unordered_map<std::string, render_line_conteiner> m_default_render_object_queue;
    render_line_conteiner m_transparent_render_object_queue;
    render_line_conteiner m_outline_object_queue;

    utils::id_allocator m_selected_material_alloc;

    buffer_layout<material_data*> m_materials_layout;

    std::vector<frame_state> m_frames;

    utils::id_allocator m_objects_id;

    // UI
    shader_effect_data* m_ui_se = nullptr;
    texture_data* m_ui_txt = nullptr;
    material_data* m_ui_mat = nullptr;
    material_data* m_outline_mat = nullptr;

    struct ui_push_constants
    {
        glm::vec2 scale;
        glm::vec2 translate;
    };

    ui_push_constants m_ui_push_constants;

    // Descriptors

    VkDescriptorSet m_objects_set = VK_NULL_HANDLE;
    VkDescriptorSet m_global_set = VK_NULL_HANDLE;

    render::gpu_push_constants m_obj_config;
};
}  // namespace render

namespace glob
{
struct vulkan_render : public singleton_instance<::agea::render::vulkan_render, vulkan_render>
{
};
}  // namespace glob

}  // namespace agea
