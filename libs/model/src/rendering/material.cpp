#include "model/rendering/material.h"

#include "model/rendering/texture.h"
#include "model/caches/textures_cache.h"

#include <vulkan_render_types/vulkan_material_data.h>
#include <model_global_api/loader.h>

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(material);

bool
material::prepare_for_rendering()
{
    m_material_data = glob::render_loader::get()->load_material(get_id(), m_base_texture->get_id(),
                                                                m_base_effect);

    if (!m_material_data)
    {
        ALOG_LAZY_ERROR;
    }

    m_roughness = &m_material_data->gpu_data.roughness;
    m_metallic = &m_material_data->gpu_data.metallic;
    m_gamma = &m_material_data->gpu_data.gamma;
    m_albedo = &m_material_data->gpu_data.albedo;

    return m_material_data;
}

bool
material::construct(this_class::construct_params&)
{
    return true;
}

}  // namespace model
}  // namespace agea
