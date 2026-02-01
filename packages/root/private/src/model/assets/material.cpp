#include "packages/root/model/assets/material.h"

namespace kryga
{
namespace root
{

KRG_gen_class_cd_default(material);

bool
material::construct(this_class::construct_params&)
{
    return true;
}

texture_slot&
material::get_slot(const utils::id& slot)
{
    return m_texture_slots[slot];
}

void
material::set_slot(const utils::id& slot, const texture_slot& ts)
{
    m_texture_slots[slot] = ts;
}

}  // namespace root
}  // namespace kryga
