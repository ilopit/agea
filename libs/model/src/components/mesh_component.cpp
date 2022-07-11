#include "model/components/mesh_component.h"

#include "model/rendering/mesh.h"
#include "model/rendering/material.h"

#include "model/caches/materials_cache.h"
#include "model/caches/meshes_cache.h"

#include "vulkan_render_types/vulkan_render_data.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(mesh_component);

bool
mesh_component::construct(construct_params& c)
{
    AGEA_return_nok(base_class::construct(c));

    return true;
}

bool
mesh_component::prepare_for_rendering()
{
    AGEA_return_nok(base_class::prepare_for_rendering());

    m_render_data->material = m_material->get_material_data();
    m_render_data->mesh = m_mesh->get_mesh_data();

    return true;
}

}  // namespace model
}  // namespace agea
