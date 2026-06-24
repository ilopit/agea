#include "packages/root/model/components/input_component.h"

namespace kryga
{
namespace root
{

KRG_gen_class_cd_default(input_component);

bool
input_component::construct(construct_params& c)
{
    base_class::construct(c);
    return true;
}

}  // namespace root
}  // namespace kryga
