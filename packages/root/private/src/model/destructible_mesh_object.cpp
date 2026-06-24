#include "packages/root/model/destructible_mesh_object.h"
#include "packages/root/model/game_object.h"

#include "packages/root/model/components/destructible_mesh_component.h"

namespace kryga
{
namespace root
{

KRG_gen_class_cd_default(destructible_mesh_object);

bool
destructible_mesh_object::construct(this_class::construct_params& p)
{
    if (!base_class::construct(p))
    {
        return false;
    }

    return true;
}

}  // namespace root
}  // namespace kryga
