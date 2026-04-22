#include "packages/base/model/components/destructible_mesh_component.h"

#include "packages/base/model/assets/destructible_mesh_asset.h"

#include "core/level.h"

namespace kryga
{
namespace base
{

KRG_gen_class_cd_default(destructible_mesh_component);

bool
destructible_mesh_component::construct(construct_params& c)
{
    KRG_return_false(base_class::construct(c));

    if (c.asset_handle)
    {
        m_asset = c.asset_handle;
    }

    return true;
}

}  // namespace base
}  // namespace kryga
