#pragma once

#include "vulkan_render/types/vulkan_render_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_light_data.h"
#include "vulkan_render/utils/vulkan_buffer.h"
#include "vulkan_render/utils/segments.h"
#include "vulkan_render/types/vulkan_render_pass.h"
#include "vulkan_render/render_cache.h"

#include <utils/singleton_instance.h>
#include <utils/id.h>
#include <utils/line_container.h>
#include <utils/id_allocator.h>

#include <vector>

namespace agea
{
namespace render
{

class vulkan_render_loader;
class render_device;
struct frame_data;
class shader_effect_data;

using render_line_container = ::agea::utils::line_container<render::vulkan_render_data*>;

using materials_update_queue = ::agea::utils::line_container<render::material_data*>;
using materials_update_queue_set = ::agea::utils::line_container<materials_update_queue>;
using objects_update_queue = ::agea::utils::line_container<render::vulkan_render_data*>;

using directional_light_update_queue =
    ::agea::utils::line_container<render::vulkan_directional_light_data*>;
using point_light_update_queue = ::agea::utils::line_container<render::vulkan_point_light_data*>;
using spot_light_update_queue = ::agea::utils::line_container<render::vulkan_spot_light_data*>;

struct pipeline_ctx
{
    uint32_t cur_material_type_idx = INVALID_GPU_INDEX;
    uint32_t cur_material_idx = INVALID_GPU_INDEX;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
};

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
        return !m_spot_light_queue.empty() || !m_dir_light_queue.empty() ||
               !m_point_light_queue.empty();
    }

    void
    reset_light_data()
    {
        m_spot_light_queue.clear();
        m_dir_light_queue.clear();
        m_point_light_queue.clear();
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
        m_point_light_queue.clear();
        m_dir_light_queue.clear();
        m_spot_light_queue.clear();

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

    directional_light_update_queue m_dir_light_queue;
    point_light_update_queue m_point_light_queue;
    spot_light_update_queue m_spot_light_queue;

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
    init(uint32_t w, uint32_t h, bool only_rp = false);

    void
    deinit();

    void
    set_camera(render::gpu_camera_data d);

    void
    draw_main();

    void
    schedule_to_drawing(render::vulkan_render_data* obj_data);

    void
    reschedule_to_drawing(render::vulkan_render_data* obj_data);

    void
    remove_from_drawing(render::vulkan_render_data* obj_data);

    void
    add_material(render::material_data* obj_data);

    void
    drop_material(render::material_data* obj_data);

    void
    schedule_material_data_gpu_upload(render::material_data* md);

    void
    schedule_game_data_gpu_upload(render::vulkan_render_data* od);

    void
    schedule_light_data_gpu_upload(render::vulkan_directional_light_data* ld);

    void
    schedule_light_data_gpu_upload(render::vulkan_spot_light_data* ld);

    void
    schedule_light_data_gpu_upload(render::vulkan_point_light_data* ld);

    void
    clear_upload_queue();

    void
    collect_lights();

    vulkan_render_data*
    object_id_under_coordinate(uint32_t x, uint32_t y);

    render_cache&
    get_cache();

    render_pass*
    get_render_pass(const utils::id& id);

private:
    void
    draw_objects(render::frame_state& frame);

    void
    prepare_draw_resources(render::frame_state& frame);

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
    draw_multi_pipeline_objects_queue(render_line_container& r,
                                      VkCommandBuffer cmd,
                                      render::frame_state& current_frame);

    void
    draw_objects_queue(render_line_container& r,
                       VkCommandBuffer cmd,
                       render::frame_state& current_frame,
                       bool outlined);

    void
    draw_same_pipeline_objects_queue(VkCommandBuffer cmd,
                                     const pipeline_ctx& pctx,
                                     const render_line_container& r,
                                     bool rebind_images = true);

    void
    draw_object(VkCommandBuffer cmd,
                const pipeline_ctx& pctx,
                const render::vulkan_render_data* obj);

    void
    bind_mesh(VkCommandBuffer cmd, mesh_data* cur_mesh);

    void
    bind_material(VkCommandBuffer cmd,
                  material_data* cur_material,
                  render::frame_state& current_frame,
                  pipeline_ctx& ctx,
                  bool outline = false,
                  bool object = true);

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

    void
    prepare_render_passes();
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

    glm::vec3 m_last_camera_position = glm::vec3{0.f};

    std::unordered_map<std::string, render_line_container> m_default_render_object_queue;

    std::unordered_map<std::string, render_line_container> m_outline_render_object_queue;

    render_line_container m_transparent_render_object_queue;

    utils::id_allocator m_selected_material_alloc;

    buffer_layout<material_data*> m_materials_layout;

    std::vector<frame_state> m_frames;

    // UI
    shader_effect_data* m_ui_se = nullptr;
    shader_effect_data* m_ui_copy_se = nullptr;

    texture_data* m_ui_txt = nullptr;
    texture_data* m_ui_target_txt = nullptr;
    material_data* m_ui_mat = nullptr;
    material_data* m_ui_target_mat = nullptr;

    struct ui_push_constants
    {
        glm::vec2 scale;
        glm::vec2 translate;
    };
    ui_push_constants m_ui_push_constants;

    // Generic
    material_data* m_outline_mat = nullptr;
    material_data* m_pick_mat = nullptr;

    // Descriptors

    VkDescriptorSet m_objects_set = VK_NULL_HANDLE;
    VkDescriptorSet m_global_set = VK_NULL_HANDLE;

    render::gpu_push_constants m_obj_config;

    std::unordered_map<utils::id, render_pass_sptr> m_render_passes;

    render_cache m_cache;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
};
}  // namespace render

namespace glob
{
struct vulkan_render : public singleton_instance<::agea::render::vulkan_render, vulkan_render>
{
};
}  // namespace glob

}  // namespace agea
