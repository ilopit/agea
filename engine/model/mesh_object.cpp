#include "model/game_object.h"

#include "model/mesh_object.h"
#include "model/components/mesh_component.h"

#include <json/json.h>

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(mesh_object);

bool
mesh_object::construct(this_class::construct_params& p)
{
    base_class::construct(p);

    return true;
}

}  // namespace model
}  // namespace agea
