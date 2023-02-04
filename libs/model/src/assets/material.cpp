#include "model/assets/material.h"

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

texture_sample&
material::get_sample(const utils::id& slot)
{
    return m_texture_samples[slot];
}

}  // namespace model
}  // namespace agea
