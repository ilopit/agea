#include "root/smart_object.h"

#include <core/reflection/reflection_type.h>

namespace agea
{
namespace root
{

AGEA_gen_class_cd_default(smart_object);

bool
smart_object::post_construct()
{
    set_state(smart_object_state::constructed);
    return true;
}

bool
smart_object::post_load()
{
    set_state(smart_object_state::constructed);
    return true;
}

core::architype
smart_object::get_architype_id() const
{
    return m_rt->arch;
}

const agea::utils::id&
smart_object::get_type_id() const
{
    return m_rt->type_name;
}

}  // namespace root
}  // namespace agea
