#pragma once

#include <vulkan_render_types/vulkan_render_fwds.h>

#include <utils/id.h>
#include <utils/path.h>
#include <utils/weird_singletone.h>

namespace agea
{
namespace model
{
class reder_loader
{
public:
    virtual render::mesh_data*
    load_mesh(const agea::utils::id& mesh_id,
              const agea::utils::path& external_path,
              const agea::utils::path& indeces_path,
              const agea::utils::path& vertices_path) = 0;

    virtual render::texture_data*
    load_texture(const agea::utils::id& texture_id, const std::string& base_color) = 0;

    virtual render::material_data*
    load_material(const agea::utils::id& material_id,
                  const agea::utils::id& texture_id,
                  const agea::utils::id& base_effect_id) = 0;

    virtual render::shader_data*
    load_shader(const agea::utils::id& path) = 0;

    virtual void
    clear_caches() = 0;

    virtual ~reder_loader();
};

}  // namespace model

namespace glob
{
struct render_loader : public simple_singleton<::agea::model::reder_loader*>
{
};
};  // namespace glob

}  // namespace agea
