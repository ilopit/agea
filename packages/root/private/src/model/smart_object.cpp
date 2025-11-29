#include "packages/root/model/smart_object.h"

#include <core/reflection/reflection_type.h>

#include <utils/defines_utils.h>
#include <utils/agea_log.h>

namespace agea
{
namespace root
{

AGEA_gen_class_cd_default(smart_object);

bool
smart_object::post_construct()
{
    ALOG_INFO("post_construct {}", get_id().cstr());

    set_state(smart_object_state::constructed);
    return true;
}

bool
smart_object::post_load()
{
    ALOG_INFO("post_construct {}", get_id().cstr());

    set_state(smart_object_state::constructed);
    return true;
}

void
smart_object::set_state(smart_object_state v)
{
    AGEA_check(m_obj_state != v, "Should go to the same state");

    m_obj_state = v;
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
