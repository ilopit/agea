#include "model/rendering/mesh.h"

#include "vulkan_render/render_loader.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(mesh);

bool
mesh::prepare_for_rendering()
{
    m_mesh_data = glob::render_loader::get()->load_mesh(*this);
    return true;
}

}  // namespace model
}  // namespace agea
