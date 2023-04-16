#include "core/package.h"

#include "core/caches/empty_objects_cache.h"
#include "core/caches/objects_cache.h"
#include "core/caches/caches_map.h"

#include "core/object_load_context.h"
#include "core/object_constructor.h"

#include <utils/agea_log.h>

#include <serialization/serialization.h>

#include <map>
#include <filesystem>

namespace agea
{
namespace core
{

package::package(package&&) noexcept = default;

package&
package::operator=(package&&) noexcept = default;

package::package(const utils::id& id, cache_set* class_global_set, cache_set* instance_global_set)
    : m_occ(std::make_unique<object_load_context>())
    , m_mapping(std::make_shared<core::object_mapping>())
    , m_id(id)
{
    m_occ->set_package(this)
        .set_class_local_set(&m_class_local_set)
        .set_instance_local_set(&m_instance_local_set)
        .set_ownable_cache(&m_objects)
        .set_class_global_set(class_global_set)
        .set_instance_global_set(instance_global_set);
}

package::~package()
{
}

utils::path
package::get_relative_path(const utils::path& p) const
{
    return p.relative(m_save_root_path);
}

void
package::init_global_cache_reference(
    cache_set* class_global_set /*= glob::class_objects_cache_set::get()*/,
    cache_set* instance_global_set /*= glob::objects_cache_set::get()*/)
{
    AGEA_check(!(m_class_global_set || m_instance_global_set), "Should be empty!");
    AGEA_check(class_global_set && instance_global_set, "Should NOT be empty!");

    m_class_global_set = class_global_set;
    m_instance_global_set = instance_global_set;

    m_occ->set_class_global_set(m_class_global_set).set_instance_global_set(m_instance_global_set);
}

void
package::register_in_global_cache()
{
    for (auto& i : m_class_local_set.objects->get_items())
    {
        auto& obj = *i.second;

        AGEA_check(obj.has_flag(root::smart_object_state_flag::proto_obj), "Should be only proto!");

        m_class_global_set->map->add_item(obj);
    }

    ALOG_INFO("[PKG:{0}], Registered {1} class object", m_id.cstr(),
              m_class_local_set.objects->get_size());

    for (auto& i : m_instance_local_set.objects->get_items())
    {
        auto& obj = *i.second;
        AGEA_check(!obj.has_flag(root::smart_object_state_flag::proto_obj) &&
                       obj.has_flag(root::smart_object_state_flag::mirror),
                   "Should NOT be only proto!");

        m_instance_global_set->map->add_item(obj);
    }

    ALOG_INFO("[PKG:{0}], Registered {1} instance object", m_id.cstr(),
              m_instance_local_set.objects->get_size());

    m_occ->set_global_load_mode(true);
}

}  // namespace core
}  // namespace agea