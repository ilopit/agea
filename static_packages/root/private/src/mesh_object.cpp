#include "packages/root/game_object.h"

#include "packages/root/mesh_object.h"
#include "packages/root/components/mesh_component.h"

namespace agea
{
namespace root
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

}  // namespace root
}  // namespace agea
