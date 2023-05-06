#include "root/components/mesh_component.h"

#include "core/level.h"

namespace agea
{
namespace root
{

AGEA_gen_class_cd_default(mesh_component);

bool
mesh_component::construct(construct_params& c)
{
    AGEA_return_false(base_class::construct(c));

    return true;
}

}  // namespace root
}  // namespace agea
