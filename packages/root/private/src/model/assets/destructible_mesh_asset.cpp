#include "packages/root/model/assets/destructible_mesh_asset.h"

namespace kryga
{
namespace root
{

KRG_gen_class_cd_default(destructible_mesh_asset);

bool
destructible_mesh_asset::construct(this_class::construct_params& p)
{
    if (!base_class::construct(p))
    {
        return false;
    }

    if (p.source_mesh)
    {
        m_source_mesh = p.source_mesh;
    }
    if (p.material_handle)
    {
        m_material = p.material_handle;
    }

    return true;
}

}  // namespace root
}  // namespace kryga
