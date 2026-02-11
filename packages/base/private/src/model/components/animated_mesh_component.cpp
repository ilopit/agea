#include "packages/base/model/components/animated_mesh_component.h"

#include "core/level.h"

namespace kryga
{
namespace base
{

KRG_gen_class_cd_default(animated_mesh_component);

bool
animated_mesh_component::construct(construct_params& c)
{
    KRG_return_false(base_class::construct(c));

    if (c.gltf.size() > 0)
    {
        m_gltf = std::move(c.gltf);
    }

    if (c.material_handle)
    {
        m_material = c.material_handle;
    }

    return true;
}

}  // namespace base
}  // namespace kryga
