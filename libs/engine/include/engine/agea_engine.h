#pragma once

#include "scene_builder.h"

#include <vulkan_render_types/vulkan_render_data.h>
#include <vulkan_render_types/vulkan_types.h>
#include <vulkan_render_types/vulkan_gpu_types.h>

#include <resource_locator/resource_locator.h>

#include <utils/singleton_instance.h>
#include <utils/id.h>
#include <utils/line_conteiner.h>

#include <model/model_fwds.h>

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
struct shader_effect_data;
class transit_buffer;
}  // namespace render

class vulkan_engine
{
    using render_line_conteiner = utils::line_conteiner<render::object_data*>;

    using materials_update_queue = utils::line_conteiner<render::material_data*>;
    using materials_update_queue_set = utils::line_conteiner<materials_update_queue>;
    using objects_update_queue = utils::line_conteiner<render::object_data*>;

    struct gpu_transfer_data
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

        materials_update_queue_set m_materias_queue_set;
        bool has_materials = false;
        objects_update_queue m_objects_queue;
    };

public:
    vulkan_engine(std::unique_ptr<singleton_registry> r);
    ~vulkan_engine();

    // initializes everything in the engine
    bool
    init();
    void
    cleanup();

    void
    draw();
    void
    run();
    void
    tick(float dt);

    void
    draw_new_objects(VkCommandBuffer cmd, render::frame_data& frame);

    void
    draw_objects(render_line_conteiner& r,
                 VkCommandBuffer cmd,
                 render::transit_buffer& obj_tb,
                 VkDescriptorSet obj_ds,
                 render::transit_buffer& dyn_tb,
                 VkDescriptorSet global_ds);

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
    update_cameras();

    void
    init_scene();

    void
    event_reload_world();

    void
    compile_all_shaders();

    void
    update_gpu_object_data(render::gpu_object_data* object_SSBO);

    void
    update_gpu_materials_data(uint8_t* object_SSBO, materials_update_queue& queue);

    bool
    load_level(const utils::id& level_id);

    void
    update_transparent_objects_queue();

    gpu_transfer_data&
    get_current_frame_transfer_data();

    // clang-format off
    void consume_updated_transforms();    
    void consume_updated_render_components();
    void consume_updated_render_assets();
    void consume_updated_shader_effects();

    bool prepare_for_rendering(model::package& p);
    bool prepare_for_rendering(model::level& l);
    bool schedule_for_rendering(model::level& l);

    std::unique_ptr<singleton_registry> m_registry;
    // clang-format on

    render::gpu_scene_data m_scene_parameters;
    render::gpu_camera_data m_camera_data;

    std::unique_ptr<scene_builder> m_scene;
    glm::vec3 m_last_camera_position = glm::vec3{0.f};

    bool m_transform_state_updated = true;

    std::unordered_map<std::string, render_line_conteiner> m_default_render_objec_queue;
    render_line_conteiner m_transparent_render_object_queue;

    std::vector<gpu_transfer_data> m_transfer_queue;
    agea::utils::line_conteiner<std::pair<uint32_t, uint32_t>> m_ssbo_range;
};

namespace glob
{
struct engine : public singleton_instance<::agea::vulkan_engine, engine>
{
};
}  // namespace glob

}  // namespace agea
