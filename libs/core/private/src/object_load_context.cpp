#pragma once

#include "core/object_load_context.h"

#include <packages/root/smart_object.h>

#include "core/caches/objects_cache.h"
#include "core/caches/textures_cache.h"
#include "core/caches/materials_cache.h"
#include "core/caches/meshes_cache.h"
#include "core/caches/caches_map.h"
#include "core/package.h"

#include "utils/agea_log.h"
#include "utils/check.h"

namespace agea
{
namespace core
{

object_load_context::object_load_context()
    : m_path_prefix()
    , m_proto_global_set()
    , m_proto_local_set()
    , m_instance_global_set()
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
object_load_context::add_obj(std::shared_ptr<root::smart_object> obj, bool add_global)
{
    AGEA_check(m_ownable_cache_ptr, "Should exists!");
    AGEA_check(obj, "Should exists!");

    auto& obj_ref = *obj.get();

    m_ownable_cache_ptr->emplace_back(std::move(obj));

    switch (m_construction_type)
    {
    case object_load_type::class_obj:
    {
        m_proto_local_set->map->add_item(obj_ref);

        if (add_global)
        {
            m_proto_global_set->map->add_item(obj_ref);
        }

        break;
    }
    case object_load_type::instance_obj:
    case object_load_type::mirror_copy:
    {
        m_instance_local_set->map->add_item(obj_ref);

        if (add_global)
        {
            m_instance_global_set->map->add_item(obj_ref);
        }
        break;
    }
    default:
        AGEA_never("Unsupported type type");
        break;
    }

    return true;
}

root::smart_object*
object_load_context::find_proto_obj(const utils::id& id)
{
    auto obj = m_proto_local_set ? m_proto_local_set->objects->get_item(id) : nullptr;

    if (obj)
    {
        return obj;
    }

    return m_proto_global_set ? m_proto_global_set->objects->get_item(id) : nullptr;
}

root::smart_object*
object_load_context::find_obj(const utils::id& id)
{
    auto obj =
        m_instance_local_set->objects ? m_instance_local_set->objects->get_item(id) : nullptr;

    if (obj)
    {
        return obj;
    }

    return m_instance_global_set ? m_instance_global_set->objects->get_item(id) : nullptr;
}

root::smart_object*
object_load_context::find_obj(const utils::id& id, architype a_type)
{
    root::smart_object* obj = nullptr;
    auto c = m_instance_local_set ? m_instance_local_set->map->get_cache(a_type) : nullptr;

    if (c)
    {
        obj = c->get_item(id);
        if (obj)
        {
            return obj;
        }
    }

    c = m_instance_global_set ? m_instance_global_set->map->get_cache(a_type) : nullptr;
    if (c)
    {
        obj = c->get_item(id);
    }

    return obj;
}

root::smart_object*
object_load_context::find_proto_obj(const utils::id& id, architype a_type)
{
    root::smart_object* obj = nullptr;
    auto c = m_proto_local_set ? m_proto_local_set->map->get_cache(a_type) : nullptr;

    if (c)
    {
        obj = c->get_item(id);
        if (obj)
        {
            return obj;
        }
    }

    c = m_proto_global_set ? m_proto_global_set->map->get_cache(a_type) : nullptr;
    if (c)
    {
        obj = c->get_item(id);
    }

    return obj;
}

}  // namespace core
}  // namespace agea