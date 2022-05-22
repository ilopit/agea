#include "model/components/mesh_component.h"

#include "model/rendering/mesh.h"
#include "model/rendering/material.h"

#include "model/caches/materials_cache.h"
#include "model/caches/meshes_cache.h"

#include "core/vk_engine.h"

#include "vulkan_render/render_loader.h"
#include "vulkan_render/vulkan_render_data.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(mesh_component);

bool
mesh_component::construct(construct_params& c)
{
    AGEA_return_nok(base_class::construct(c));

    // m_material = glob::materials_cache::get()->get(c.material_id);
    // m_mesh = glob::meshes_cache::get()->get(c.mesh_id).;

    return true;
}

bool
mesh_component::prepare_for_rendering()
{
    AGEA_return_nok(base_class::prepare_for_rendering());

    m_render_data->material = m_material->m_material;
    m_render_data->mesh = m_mesh->m_mesh_data;

    return true;
}

}  // namespace model
}  // namespace agea
