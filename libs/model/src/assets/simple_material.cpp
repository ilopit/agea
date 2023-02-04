#include "model/assets/simple_material.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(simple_material);

bool
simple_material::construct(this_class::construct_params&)
{
    return true;
}

bool
simple_material::post_construct()
{
    AGEA_return_nok(base_class::post_construct());

    return true;
}

}  // namespace model
}  // namespace agea
