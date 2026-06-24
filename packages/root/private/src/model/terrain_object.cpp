#include "packages/root/model/terrain_object.h"
#include "packages/root/model/game_object.h"

#include "packages/root/model/components/terrain_component.h"

namespace kryga
{
namespace root
{

KRG_gen_class_cd_default(terrain_object);

bool
terrain_object::construct(this_class::construct_params& p)
{
    if (!base_class::construct(p))
    {
        return false;
    }

    return true;
}

}  // namespace root
}  // namespace kryga
