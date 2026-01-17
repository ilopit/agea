#pragma once

#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/types/vulkan_light_data.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_render_pass.h"
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

namespace kryga
{
namespace render
{
class render_device;

class vulkan_render_loader
{
public:
    /*************************/

    mesh_data*
    get_mesh_data(const kryga::utils::id& id)
    {
        return get_data<mesh_data>(m_meshes_cache, id);
    }

    mesh_data*
    create_mesh(const kryga::utils::id& mesh_id,
                kryga::utils::buffer_view<gpu::vertex_data> vertices,
                kryga::utils::buffer_view<gpu::uint> indices);

    void
    destroy_mesh_data(const kryga::utils::id& id);

    /*************************/

    texture_data*
    get_texture_data(const kryga::utils::id& id)
    {
        return get_data<texture_data>(m_textures_cache, id);
    }

    texture_data*
    create_texture(const kryga::utils::id& texture_id,
                   const kryga::utils::buffer& base_color,
                   uint32_t w,
                   uint32_t h);

    texture_data*
    create_texture(const kryga::utils::id& texture_id,
                   vk_utils::vulkan_image_sptr image,
                   vk_utils::vulkan_image_view_sptr view);

    void
    destroy_texture_data(const kryga::utils::id& id);

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
    get_material_data(const kryga::utils::id& id)
    {
        return get_data<material_data>(m_materials_cache, id);
    }

    std::unordered_map<kryga::utils::id, std::shared_ptr<material_data>>&
    get_materials_cache()
    {
        return m_materials_cache;
    }

    material_data*
    create_material(const kryga::utils::id& id,
                    const kryga::utils::id& type_id,
                    std::vector<texture_sampler_data>& textures_data,
                    shader_effect_data& se_data,
                    const kryga::utils::dynobj& params);

    bool
    update_material(material_data& mat_data,
                    std::vector<texture_sampler_data>& textures_data,
                    shader_effect_data& se_data,
                    const kryga::utils::dynobj& params);

    void
    destroy_material_data(const kryga::utils::id& id);

    /*************************/
    void
    create_font(const kryga::utils::id& id, ImFont* font);

    ImFont*
    get_font(const kryga::utils::id& id)
    {
        auto itr = m_fonts_cache.find(id);

        return itr != m_fonts_cache.end() ? itr->second : nullptr;
    }

    /*************************/
    sampler_data*
    get_sampler_data(const kryga::utils::id& id)
    {
        return get_data<sampler_data>(m_samplers_cache, id);
    }

    sampler_data*
    create_sampler(const kryga::utils::id& id, VkBorderColor color);

    void
    destroy_sampler_data(const kryga::utils::id& id);

    /*************************/
    render_pass*
    get_render_pass(const kryga::utils::id& id)
    {
        auto itr = m_render_passes.find(id);
        return itr != m_render_passes.end() ? itr->second.get() : nullptr;
    }

    void
    add_render_pass(const kryga::utils::id& id, render_pass_sptr rp)
    {
        m_render_passes[id] = std::move(rp);
    }

    void
    destroy_render_pass(const kryga::utils::id& id);

    /*************************/
    void
    clear_caches();

    template <typename T>
    static T*
    get_data(std::unordered_map<kryga::utils::id, std::shared_ptr<T>>& col,
             const kryga::utils::id& id)
    {
        auto itr = col.find(id);

        return itr != col.end() ? itr->second.get() : nullptr;
    }

    gpu_data_index_type
    last_mat_index(const kryga::utils::id& type_id)
    {
        return m_materials_index.at(type_id);
    }

private:
    gpu_data_index_type
    generate_mt_idx(const kryga::utils::id& type_id)
    {
        return m_materials_index[type_id]++;
    }

    std::unordered_map<kryga::utils::id, std::shared_ptr<mesh_data>> m_meshes_cache;
    std::unordered_map<kryga::utils::id, std::shared_ptr<texture_data>> m_textures_cache;
    std::unordered_map<kryga::utils::id, std::shared_ptr<material_data>> m_materials_cache;
    std::unordered_map<kryga::utils::id, std::shared_ptr<shader_module_data>> m_shaders_cache;

    std::unordered_map<kryga::utils::id, std::shared_ptr<sampler_data>> m_samplers_cache;

    std::unordered_map<kryga::utils::id, render_pass_sptr> m_render_passes;

    std::unordered_map<kryga::utils::id, gpu_data_index_type> m_materials_index;

    std::unordered_map<kryga::utils::id, ImFont*> m_fonts_cache;
};

}  // namespace render

namespace glob
{
struct vulkan_render_loader
    : public ::kryga::singleton_instance<::kryga::render::vulkan_render_loader,
                                         vulkan_render_loader>
{
};
};  // namespace glob
}  // namespace kryga
