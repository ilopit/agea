#include "packages/test/model/test_mesh_object.h"

#include <packages/base/model/components/mesh_component.h>
#include <packages/base/model/lights/components/directional_light_component.h>

#include <core/caches/cache_set.h>

namespace agea
{
namespace test
{

AGEA_gen_class_cd_default(test_mesh_object);

bool
test_mesh_object::construct(construct_params& params)
{
    if (!base_class::construct(params))
    {
        return false;
    }

    spawn_component_with_proto(m_root_component, AID("test_complex_mesh_component"));

    return true;
}

}  // namespace test
}  // namespace agea
