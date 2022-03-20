#include "model/rendering/mesh.h"

#include "vulkan_render/render_loader.h"

namespace agea
{
namespace model
{
bool
mesh::prepare_for_rendering()
{
    m_mesh_data = glob::render_loader::get()->load_mesh(*this);
    return true;
}

bool
mesh::deserialize(json_conteiner& c)
{
    AGEA_return_nok(base_class::deserialize(c));

    m_indeces_id = c["indeces"].asString();
    m_verteces_id = c["vertices"].asString();
    m_index_size = c["index_size"].asUInt();

    return true;
}

}  // namespace model
}  // namespace agea
