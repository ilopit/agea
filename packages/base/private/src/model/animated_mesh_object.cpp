#include "packages/base/model/animated_mesh_object.h"
#include "packages/root/model/game_object.h"

#include "packages/base/model/components/animated_mesh_component.h"

namespace kryga
{
namespace base
{

KRG_gen_class_cd_default(animated_mesh_object);

bool
animated_mesh_object::construct(this_class::construct_params& p)
{
    if (!base_class::construct(p))
    {
        return false;
    }

    return true;
}

}  // namespace base
}  // namespace kryga
