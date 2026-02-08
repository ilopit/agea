#include "packages/base/model/components/input_component.h"

namespace kryga
{
namespace base
{

KRG_gen_class_cd_default(input_component);

bool
input_component::construct(construct_params& c)
{
    base_class::construct(c);
    return true;
}

}  // namespace base
}  // namespace kryga
