#include "core/object_load_context.h"

#include <packages/root/model/smart_object.h>

#include "core/package.h"
#include "global_state/global_state.h"

#include "utils/agea_log.h"
#include "utils/check.h"

namespace agea
{
namespace core
{

object_load_context::object_load_context()
    : m_path_prefix()
    , m_proto_local_set()
    , m_instance_local_set()
    , m_ownable_cache_ptr(nullptr)
{
    ALOG_TRACE("Created");
}

object_load_context::~object_load_context()
{
    ALOG_TRACE("Destructed");
}

bool
object_load_context::make_full_path(const utils::path& relative_path, utils::path& p) const
{
    if (m_path_prefix.empty())
    {
        return false;
    }

    p = m_path_prefix;
    p.append(relative_path);

    return true;
}

bool
object_load_context::make_full_path(const utils::id& id, utils::path& p) const
{
    if (!m_object_mapping)
    {
        return false;
    }

    auto itr = m_object_mapping->m_items.find(id);
    if (itr == m_object_mapping->m_items.end())
    {
        return false;
    }

    return make_full_path(itr->second.p, p);
}

bool
object_load_context::add_obj(std::shared_ptr<root::smart_object> obj)
{
    AGEA_check(m_ownable_cache_ptr, "Should exists!");
    AGEA_check(obj, "Should exists!");

    auto& obj_ref = *obj.get();

    m_ownable_cache_ptr->emplace_back(std::move(obj));

    switch (get_construction_type())
    {
    case object_load_type::class_obj:
    {
        m_proto_local_set->map.add_item(obj_ref);

        glob::glob_state().get_class_cache_map()->add_item(obj_ref);

        break;
    }
    case object_load_type::instance_obj:
    {
        m_instance_local_set->map.add_item(obj_ref);
        glob::glob_state().get_instance_cache_map()->add_item(obj_ref);
        break;
    }
    default:
        AGEA_never("Unsupported type");
        break;
    }

    return true;
}

bool
object_load_context::remove_obj(const root::smart_object& obj)
{
    if (obj.get_flags().instance_obj)
    {
        if (m_instance_local_set)
        {
            m_instance_local_set->map.remove_item(obj);
        }
        glob::glob_state().get_instance_cache_map()->remove_item(obj);
    }
    else
    {
        if (m_proto_local_set)
        {
            m_proto_local_set->map.remove_item(obj);
        }
        glob::glob_state().get_class_cache_map()->remove_item(obj);
    }

    return true;
}

root::smart_object*
object_load_context::find_proto_obj(const utils::id& id)
{
    auto obj = m_proto_local_set ? m_proto_local_set->objects.get_item(id) : nullptr;

    if (!obj)
    {
        obj = glob::glob_state().get_class_objects_cache()->get_item(id);
    }

    AGEA_check(!obj || !obj->get_flags().instance_obj, "Should always be proto!");
    return obj;
}

root::smart_object*
object_load_context::find_obj(const utils::id& id)
{
    auto obj = m_instance_local_set ? m_instance_local_set->objects.get_item(id) : nullptr;

    if (!obj)
    {
        obj = glob::glob_state().get_instance_objects_cache()->get_item(id);
    }

    AGEA_check(!obj || obj->get_flags().instance_obj, "Should always be instance_obj!");
    return obj;
}

root::smart_object*
object_load_context::find_obj(const utils::id& id, architype a_type)
{
    root::smart_object* obj = nullptr;
    auto c = m_instance_local_set ? m_instance_local_set->map.get_cache(a_type) : nullptr;

    if (c)
    {
        obj = c->get_item(id);
        if (obj)
        {
            return obj;
        }
    }

    obj = glob::glob_state().get_instance_objects_cache()->get_item(id);

    return obj;
}

root::smart_object*
object_load_context::find_proto_obj(const utils::id& id, architype a_type)
{
    root::smart_object* obj = nullptr;
    auto c = m_proto_local_set ? m_proto_local_set->map.get_cache(a_type) : nullptr;

    if (c)
    {
        obj = c->get_item(id);
        if (obj)
        {
            return obj;
        }
    }

    obj = glob::glob_state().get_class_objects_cache()->get_item(id);

    return obj;
}

}  // namespace core
}  // namespace agea