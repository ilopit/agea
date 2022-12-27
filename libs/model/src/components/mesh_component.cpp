#include "model/components/mesh_component.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(mesh_component);

bool
mesh_component::construct(construct_params& c)
{
    AGEA_return_nok(base_class::construct(c));

    return true;
}


}  // namespace model
}  // namespace agea
