#include "model/rendering/material.h"

#include "model/rendering/texture.h"
#include "model/caches/textures_cache.h"
#include "vulkan_render/render_loader.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(material);

bool
material::prepare_for_rendering()
{
    m_material = glob::render_loader::get()->load_material(*this);

    m_roughness = &m_material->gpu_data.roughness;
    m_metallic = &m_material->gpu_data.metallic;
    m_gamma = &m_material->gpu_data.gamma;
    m_albedo = &m_material->gpu_data.albedo;

    return true;
}

bool
material::construct(this_class::construct_params&)
{
    return true;
}

}  // namespace model
}  // namespace agea
