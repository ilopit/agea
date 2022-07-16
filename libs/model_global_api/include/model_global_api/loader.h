#pragma once

#include "vulkan_render_types/vulkan_render_fwds.h"

#include <utils/id.h>
#include <utils/path.h>
#include <utils/weird_singletone.h>

namespace agea
{
namespace render
{
class loader
{
public:
    virtual mesh_data*
    load_mesh(const agea::utils::id& mesh_id,
              const agea::utils::path& external_path,
              const agea::utils::path& indeces_path,
              const agea::utils::path& vertices_path) = 0;

    virtual texture_data*
    load_texture(const agea::utils::id& texture_id, const std::string& base_color) = 0;

    virtual material_data*
    load_material(const agea::utils::id& material_id,
                  const agea::utils::id& texture_id,
                  const agea::utils::id& base_effect_id) = 0;

    virtual shader_data*
    load_shader(const agea::utils::id& path) = 0;

    virtual void
    clear_caches() = 0;

    virtual ~loader();
};

}  // namespace render

namespace glob
{
struct render_loader : public simple_singleton<::agea::render::loader*>
{
};
};  // namespace glob

}  // namespace agea
