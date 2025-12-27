#pragma once

#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/types/vulkan_light_data.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/utils/vulkan_image.h"
#include "vulkan_render/vulkan_render_loader_create_infos.h"

#include <error_handling/error_handling.h>

#include <utils/buffer.h>
#include <utils/id.h>
#include <utils/path.h>
#include <utils/singleton_instance.h>
#include <utils/dynamic_object.h>
#include <utils/line_container.h>
#include <utils/path.h>

#include <functional>
#include <memory>
#include <unordered_map>
#include <queue>

struct ImFont;

namespace agea
{
namespace render
{
class render_device;

class vulkan_render_loader
{
public:
    /*************************/

    mesh_data*
    get_mesh_data(const agea::utils::id& id)
    {
        return get_data<mesh_data>(m_meshes_cache, id);
    }

    mesh_data*
    create_mesh(const agea::utils::id& mesh_id,
                agea::utils::buffer_view<render::gpu_vertex_data> vertices,
                agea::utils::buffer_view<render::gpu_index_data> indices);

    void
    destroy_mesh_data(const agea::utils::id& id);

    /*************************/

    texture_data*
    get_texture_data(const agea::utils::id& id)
    {
        return get_data<texture_data>(m_textures_cache, id);
    }

    texture_data*
    create_texture(const agea::utils::id& texture_id,
                   const agea::utils::buffer& base_color,
                   uint32_t w,
                   uint32_t h);

    texture_data*
    create_texture(const agea::utils::id& texture_id,
                   vk_utils::vulkan_image_sptr image,
                   vk_utils::vulkan_image_view_sptr view);

    void
    destroy_texture_data(const agea::utils::id& id);

    /*************************/

    bool
    update_object(vulkan_render_data& obj_data,
                  material_data& mat_data,
                  mesh_data& mesh_data,
                  const glm::mat4& model_matrix,
                  const glm::mat4& normal_matrix,
                  const glm::vec3& obj_pos);

    /*************************/

    material_data*
    get_material_data(const agea::utils::id& id)
    {
        return get_data<material_data>(m_materials_cache, id);
    }

    std::unordered_map<agea::utils::id, std::shared_ptr<material_data>>&
    get_materials_cache()
    {
        return m_materials_cache;
    }

    material_data*
    create_material(const agea::utils::id& id,
                    const agea::utils::id& type_id,
                    std::vector<texture_sampler_data>& textures_data,
                    shader_effect_data& se_data,
                    const agea::utils::dynobj& params);

    bool
    update_material(material_data& mat_data,
                    std::vector<texture_sampler_data>& textures_data,
                    shader_effect_data& se_data,
                    const agea::utils::dynobj& params);

    void
    destroy_material_data(const agea::utils::id& id);

    /*************************/
    shader_effect_data*
    get_shader_effect_data(const agea::utils::id& id)
    {
        return get_data<shader_effect_data>(m_shaders_effects_cache, id);
    }

    ::agea::result_code
    create_shader_effect(const agea::utils::id& id,
                         const shader_effect_create_info& info,
                         shader_effect_data*& sed);

    ::agea::result_code
    update_shader_effect(shader_effect_data& se_data, const shader_effect_create_info& info);

    void
    destroy_shader_effect_data(const agea::utils::id& id);

    /*************************/
    void
    create_font(const agea::utils::id& id, ImFont* font);

    ImFont*
    get_font(const agea::utils::id& id)
    {
        auto itr = m_fonts_cache.find(id);

        return itr != m_fonts_cache.end() ? itr->second : nullptr;
    }

    /*************************/
    sampler_data*
    get_sampler_data(const agea::utils::id& id)
    {
        return get_data<sampler_data>(m_samplers_cache, id);
    }

    sampler_data*
    create_sampler(const agea::utils::id& id, VkBorderColor color);

    void
    destroy_sampler_data(const agea::utils::id& id);

    /*************************/
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

    gpu_data_index_type
    last_mat_index(const agea::utils::id& type_id)
    {
        return m_materials_index.at(type_id);
    }

private:
    gpu_data_index_type
    generate_mt_idx(const agea::utils::id& type_id)
    {
        return m_materials_index[type_id]++;
    }

    std::unordered_map<agea::utils::id, std::shared_ptr<mesh_data>> m_meshes_cache;
    std::unordered_map<agea::utils::id, std::shared_ptr<texture_data>> m_textures_cache;
    std::unordered_map<agea::utils::id, std::shared_ptr<material_data>> m_materials_cache;
    std::unordered_map<agea::utils::id, std::shared_ptr<shader_module_data>> m_shaders_cache;

    std::unordered_map<agea::utils::id, std::shared_ptr<shader_effect_data>>
        m_shaders_effects_cache;

    std::unordered_map<agea::utils::id, std::shared_ptr<sampler_data>> m_samplers_cache;

    std::unordered_map<agea::utils::id, gpu_data_index_type> m_materials_index;

    std::unordered_map<agea::utils::id, ImFont*> m_fonts_cache;
};

}  // namespace render

namespace glob
{
struct vulkan_render_loader
    : public ::agea::singleton_instance<::agea::render::vulkan_render_loader, vulkan_render_loader>
{
};
};  // namespace glob
}  // namespace agea
