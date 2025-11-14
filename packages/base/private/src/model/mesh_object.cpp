#include "packages/base/model/mesh_object.h"
#include "packages/root/model/game_object.h"

#include "packages/base/model/components/mesh_component.h"

namespace agea
{
namespace base
{

AGEA_gen_class_cd_default(mesh_object);

bool
mesh_object::construct(this_class::construct_params& p)
{
    if (!base_class::construct(p))
    {
        return false;
    }

    return true;
}

}  // namespace base
}  // namespace agea
