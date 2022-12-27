#include "model/game_object.h"

#include "model/mesh_object.h"
#include "model/components/mesh_component.h"

namespace agea
{
namespace model
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

}  // namespace model
}  // namespace agea
