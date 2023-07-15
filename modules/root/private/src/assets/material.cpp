#include "root/assets/material.h"

namespace agea
{
namespace root
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

void
material::set_sample(const utils::id& slot, const texture_sample& ts)
{
    m_texture_samples[slot] = ts;
}

}  // namespace root
}  // namespace agea
