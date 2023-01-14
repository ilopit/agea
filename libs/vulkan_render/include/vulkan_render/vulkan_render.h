#pragma once

#include "vulkan_render/types/vulkan_render_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/utils/vulkan_buffer.h"
#include "vulkan_render/utils/vulkan_image.h"

#include <resource_locator/resource_locator.h>

#include <utils/singleton_instance.h>
#include <utils/id.h>
#include <utils/line_conteiner.h>

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

struct frame_state
{
    bool
    has_data() const
    {
        return has_materials || !m_objects_queue.empty();
    }

    void
    reset()
    {
        has_materials = false;
    }

    vk_utils::vulkan_buffer m_object_buffer;
    vk_utils::vulkan_buffer m_dynamic_data_buffer;

    vk_utils::vulkan_buffer m_ui_vertex_buffer;
    vk_utils::vulkan_buffer m_ui_index_buffer;

    int32_t m_ui_vertex_count = 0;
    int32_t m_ui_index_count = 0;

    objects_update_queue m_objects_queue;
    materials_update_queue_set m_materias_queue_set;

    bool has_materials = false;
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
    schedule_material_data_gpu_transfer(render::material_data* md);

    void
    schedule_game_data_gpu_transfer(render::object_data* md);

    void
    update_ssbo_data_ranges(render::gpu_data_index_type range_id);

private:
    void
    draw_objects(render::frame_state& frame);

    void
    draw_objects_queue(render_line_conteiner& r,
                       VkCommandBuffer cmd,
                       vk_utils::vulkan_buffer& obj_tb,
                       VkDescriptorSet obj_ds,
                       vk_utils::vulkan_buffer& dyn_tb,
                       VkDescriptorSet global_ds);

    void
    update_gpu_object_data(render::gpu_object_data* object_SSBO);

    void
    update_gpu_materials_data(uint8_t* object_SSBO, materials_update_queue& queue);

    void
    update_transparent_objects_queue();

    frame_state&
    get_current_frame_transfer_data();

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

    std::unordered_map<std::string, render_line_conteiner> m_default_render_object_queue;
    render_line_conteiner m_transparent_render_object_queue;

    std::vector<frame_state> m_frames;
    agea::utils::line_conteiner<std::pair<uint32_t, uint32_t>> m_ssbo_range;

    // UI

    render::shader_effect_data* m_ui_se = nullptr;
    render::texture_data* m_ui_txt = nullptr;

    struct ui_push_constants
    {
        glm::vec2 scale;
        glm::vec2 translate;
    };

    ui_push_constants m_ui_push_constants;
};
}  // namespace render

namespace glob
{
struct vulkan_render : public singleton_instance<::agea::render::vulkan_render, vulkan_render>
{
};
}  // namespace glob

}  // namespace agea
