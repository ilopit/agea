#pragma once

#include "vulkan_material_data.h"

#include "utils/id.h"

#include "utils/weird_singletone.h"
#include "vulkan_render/vulkan_render_fwds.h"

#include <memory>
#include <unordered_map>

namespace agea
{
namespace model
{
class mesh;
class texture;
class material;
}  // namespace model

namespace render
{
class loader
{
public:
    mesh_data*
    load_mesh(model::mesh& m);
    texture_data*
    load_texture(model::texture& t);
    material_data*
    load_material(model::material& d);
    shader_data*
    load_shader(const utils::id& path);

    void
    clear_caches();

    bool
    create_default_material(VkPipeline pipeline, shader_effect* effect, const utils::id& id);

private:
    std::unordered_map<utils::id, std::shared_ptr<mesh_data>> m_meshes_cache;
    std::unordered_map<utils::id, std::shared_ptr<texture_data>> m_textures_cache;
    std::unordered_map<utils::id, std::shared_ptr<material_data>> m_materials_cache;
    std::unordered_map<utils::id, std::shared_ptr<shader_data>> m_shaders_cache;
};

}  // namespace render

namespace glob
{
struct render_loader : public weird_singleton<::agea::render::loader>
{
};
};  // namespace glob

}  // namespace agea
