#include "packages/root/model/components/mesh_component.h"

#include "core/level.h"

namespace agea
{
namespace root
{

AGEA_gen_class_cd_default(mesh_component);

bool
mesh_component::construct(construct_params& c)
{
    AGEA_return_false(base_class::construct(c));

    if (c.mesh_handle)
    {
        m_mesh = c.mesh_handle;
    }

    if (c.material_handle)
    {
        m_material = c.material_handle;
    }

    return true;
}

}  // namespace root
}  // namespace agea
