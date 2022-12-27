#pragma once

#include <vulkan_render_types/vulkan_gpu_types.h>
#include <vulkan_render_types/vulkan_render_fwds.h>

#include <utils/buffer.h>
#include <utils/id.h>
#include <utils/path.h>
#include <utils/singleton_instance.h>
#include <utils/dynamic_object.h>
#include <utils/line_conteiner.h>

#include <utils/path.h>

#include <vulkan/vulkan.h>

#include <functional>
#include <memory>
#include <unordered_map>
#include <queue>

namespace agea
{
namespace render
{
class render_device;

class vulkan_loader
{
public:
    using resource_deleter = std::function<void()>;

    struct deleyer_delete_action
    {
        uint64_t frame_to_delete = 0;
        resource_deleter deleter = nullptr;
    };

    struct deleyer_delete_action_compare
    {
        bool
        operator()(const deleyer_delete_action& l, const deleyer_delete_action& r)
        {
            return l.frame_to_delete < r.frame_to_delete;
        }
    };

    mesh_data*
    create_mesh(const agea::utils::id& mesh_id,
                agea::utils::buffer_view<render::gpu_vertex_data> vertices,
                agea::utils::buffer_view<render::gpu_index_data> indices);

    texture_data*
    create_texture(const agea::utils::id& texture_id,
                   const agea::utils::buffer& base_color,
                   uint32_t w,
                   uint32_t h);

    object_data*
    create_object(const agea::utils::id& id,
                  material_data& mat_data,
                  mesh_data& mesh_data,
                  const glm::mat4& model_matrix,
                  const glm::mat4& normal_matrix,
                  const glm::vec3& obj_pos);

    bool
    update_object(object_data& obj_data,
                  material_data& mat_data,
                  mesh_data& mesh_data,
                  const glm::mat4& model_matrix,
                  const glm::mat4& normal_matrix,
                  const glm::vec3& obj_pos);

    material_data*
    create_material(const agea::utils::id& id,
                    const agea::utils::id& type_id,
                    texture_data& t_data,
                    shader_effect_data& se_data,
                    const agea::utils::dynamic_object& params);

    bool
    update_material(material_data& mat_data,
                    texture_data& t_data,
                    shader_effect_data& se_data,
                    const agea::utils::dynamic_object& params);

    shader_effect_data*
    create_shader_effect(const agea::utils::id& id,
                         agea::utils::buffer& vert_buffer,
                         bool is_vert_binary,
                         agea::utils::buffer& frag_buffer,
                         bool is_frag_binary,
                         bool is_wire,
                         bool enable_alpha,
                         VkRenderPass render_path);

    bool
    update_shader_effect(shader_effect_data& se_data,
                         agea::utils::buffer& vert_buffer,
                         bool is_vert_binary,
                         agea::utils::buffer& frag_buffer,
                         bool is_frag_binary,
                         bool is_wire,
                         bool enable_alpha,
                         VkRenderPass render_path);

    void
    clear_caches();

    template <typename T>
    static T*
    get_data(std::unordered_map<agea::utils::id, std::shared_ptr<T>>& col,
             const agea::utils::id& id)
    {
        auto itr = col.find(id);

        return itr != col.end() ? itr->second.get() : nullptr;
    }

    mesh_data*
    get_mesh_data(const agea::utils::id& id)
    {
        return get_data<mesh_data>(m_meshes_cache, id);
    }

    texture_data*
    get_texture_data(const agea::utils::id& id)
    {
        return get_data<texture_data>(m_textures_cache, id);
    }

    material_data*
    get_material_data(const agea::utils::id& id)
    {
        return get_data<material_data>(m_materials_cache, id);
    }

    shader_data*
    get_shader_data(const agea::utils::id& id)
    {
        return get_data<shader_data>(m_shaders_cache, id);
    }

    shader_effect_data*
    get_shader_effect_data(const agea::utils::id& id)
    {
        return get_data<shader_effect_data>(m_shaders_effects_cache, id);
    }

    object_data*
    get_object_data(const agea::utils::id& id)
    {
        return get_data<object_data>(m_objects_cache, id);
    }

    void
    delete_sheduled_actions();

    gpu_data_index_type
    generate_new_object_index()
    {
        return m_new_object_index++;
    }

private:
    void
    shedule_to_deltete(resource_deleter d);

    gpu_data_index_type
    generate_mt_idx(const agea::utils::id& type_id)
    {
        return m_materials_index[type_id]++;
    }

    gpu_data_index_type
    generate_mtt_id(const agea::utils::id& type_id)
    {
        auto itr = m_materials_type_ids.find(type_id);

        if (itr == m_materials_type_ids.end())
        {
            itr = m_materials_type_ids.insert({type_id, m_last_mtt_id++}).first;
        }

        return itr->second;
    }

    std::unordered_map<agea::utils::id, std::shared_ptr<mesh_data>> m_meshes_cache;
    std::unordered_map<agea::utils::id, std::shared_ptr<texture_data>> m_textures_cache;
    std::unordered_map<agea::utils::id, std::shared_ptr<material_data>> m_materials_cache;
    std::unordered_map<agea::utils::id, std::shared_ptr<shader_data>> m_shaders_cache;
    std::unordered_map<agea::utils::id, std::shared_ptr<shader_effect_data>>
        m_shaders_effects_cache;
    std::unordered_map<agea::utils::id, std::shared_ptr<object_data>> m_objects_cache;

    std::priority_queue<deleyer_delete_action,
                        std::vector<deleyer_delete_action>,
                        deleyer_delete_action_compare>
        m_ddq;

    std::unordered_map<agea::utils::id, gpu_data_index_type> m_materials_index;
    std::unordered_map<agea::utils::id, gpu_data_index_type> m_materials_type_ids;

    gpu_data_index_type m_new_object_index = 0U;
    gpu_data_index_type m_last_mtt_id = 0U;
};

}  // namespace render

namespace glob
{
struct vulkan_render_loader
    : public ::agea::singleton_instance<::agea::render::vulkan_loader, vulkan_render_loader>
{
};
};  // namespace glob

}  // namespace agea
