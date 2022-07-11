#include "model/rendering/mesh.h"

#include "model/package.h"

#include "vulkan_render/render_loader.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(mesh);

bool
mesh::prepare_for_rendering()
{
    utils::path external;

    if (!get_external_path().empty())
    {
        external = m_package->get_resource_path(get_external_path());
    }

    m_mesh_data = glob::render_loader::get()->load_mesh(get_id(), external,
                                                        m_package->get_resource_path(m_indices),
                                                        m_package->get_resource_path(m_vertices));

    if (!m_mesh_data)
    {
        ALOG_LAZY_ERROR;
    }

    return m_mesh_data;
}

}  // namespace model
}  // namespace agea
