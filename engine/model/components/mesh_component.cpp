#include "mesh_component.h"

#include "model/rendering/mesh.h"
#include "model/rendering/material.h"

#include "vulkan_render/render_loader.h"
#include "vulkan_render/vulkan_render_data.h"

#include <json/json.h>

namespace agea
{
namespace model
{
bool
mesh_component::construct(construct_params& c)
{
    AGEA_return_nok(base_class::construct(c));

    m_material_id = c.material_id;
    m_mesh_id = c.mesh_id;

    return true;
}

bool
mesh_component::clone(this_class& src)
{
    AGEA_return_nok(base_class::clone(src));

    m_material_id = src.m_material_id;
    m_mesh_id = src.m_mesh_id;

    m_mesh = src.m_mesh;
    m_material = src.m_material;

    return true;
}

bool
mesh_component::prepare_for_rendering()
{
    AGEA_return_nok(base_class::prepare_for_rendering());

    AGEA_return_nok(m_material->prepare_for_rendering());
    AGEA_return_nok(m_mesh->prepare_for_rendering());

    m_render_object->material = m_material->m_material;
    m_render_object->mesh = m_mesh->m_mesh_data;

    return true;
}

bool
mesh_component::deserialize(json_conteiner& c)
{
    AGEA_return_nok(base_class::deserialize(c));

    game_object_serialization_helper::read_if_exists<std::string>("material", c, m_material_id);
    m_material = smart_object_serializer::object_deserialize_concrete<material>(m_material_id,
                                                                                category::assets);

    game_object_serialization_helper::read_if_exists<std::string>("mesh", c, m_mesh_id);
    m_mesh =
        smart_object_serializer::object_deserialize_concrete<mesh>(m_mesh_id, category::assets);

    return true;
}

}  // namespace model
}  // namespace agea
