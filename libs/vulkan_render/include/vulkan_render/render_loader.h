#pragma once

#include "vulkan_render_types/vulkan_material_data.h"

#include <utils/id.h>
#include <utils/path.h>
#include <utils/weird_singletone.h>

#include <vulkan_render_types/vulkan_render_fwds.h>
#include <vulkan_render_types/vulkan_shader_effect.h>
#include <model_global_api/loader.h>

#include <memory>
#include <unordered_map>

namespace agea
{
namespace render
{
class render_device;

uint32_t
hash_descriptor_layout_info(VkDescriptorSetLayoutCreateInfo* info);

void
reflect_layout(render_device* engine,
               shader_effect& se,
               shader_effect::reflection_overrides* overrides,
               int overrideCount);

class vulkan_loader : public loader
{
public:
    virtual mesh_data*
    load_mesh(const agea::utils::id& mesh_id,
              const agea::utils::path& external_path,
              const agea::utils::path& indeces_path,
              const agea::utils::path& vertices_path) override;

    virtual texture_data*
    load_texture(const agea::utils::id& texture_id, const std::string& base_color) override;

    virtual material_data*
    load_material(const agea::utils::id& material_id,
                  const agea::utils::id& texture_id,
                  const agea::utils::id& base_effect_id) override;
    virtual shader_data*
    load_shader(const agea::utils::id& path) override;

    virtual void
    clear_caches() override;

    bool
    create_default_material(VkPipeline pipeline, shader_effect* effect, const agea::utils::id& id);

private:
    std::unordered_map<agea::utils::id, std::shared_ptr<mesh_data>> m_meshes_cache;
    std::unordered_map<agea::utils::id, std::shared_ptr<texture_data>> m_textures_cache;
    std::unordered_map<agea::utils::id, std::shared_ptr<material_data>> m_materials_cache;
    std::unordered_map<agea::utils::id, std::shared_ptr<shader_data>> m_shaders_cache;
};

}  // namespace render

namespace glob
{
struct vulkan_render_loader : public ::agea::selfcleanable_singleton<::agea::render::vulkan_loader>
{
};
};  // namespace glob

}  // namespace agea
