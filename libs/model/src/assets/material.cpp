#include "model/assets/material.h"

#include "model/assets/texture.h"
#include "model/caches/textures_cache.h"
#include "model/package.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(material);

bool
material::construct(this_class::construct_params&)
{
    return true;
}

}  // namespace model
}  // namespace agea
