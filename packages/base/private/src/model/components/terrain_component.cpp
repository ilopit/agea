#include "packages/base/model/components/terrain_component.h"

#include "core/level.h"

namespace kryga
{
namespace base
{

KRG_gen_class_cd_default(terrain_component);

bool
terrain_component::construct(construct_params& c)
{
    KRG_return_false(base_class::construct(c));
    return true;
}

}  // namespace base
}  // namespace kryga
