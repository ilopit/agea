#pragma once

#include "model/object_construction_context.h"

#include "model/smart_object.h"
#include "model/caches/class_object_cache.h"
#include "model/caches/game_objects_cache.h"
#include "model/caches/objects_cache.h"
#include "model/caches/textures_cache.h"
#include "model/caches/materials_cache.h"
#include "model/caches/meshes_cache.h"
#include "model/package.h"

#include "utils/agea_log.h"
#include "utils/check.h"

namespace agea
{
namespace model
{

object_constructor_context::object_constructor_context(const utils::path& prefix_path,
                                                       cache_set_ref class_global_map,
                                                       cache_set_ref class_local_map,
                                                       cache_set_ref instance_global_map,
                                                       cache_set_ref instance_local_map,
                                                       line_cache* ownable_cache)
    : m_path_prefix(prefix_path)
    , m_class_global_set(class_global_map)
    , m_class_local_set(class_local_map)
    , m_instance_global_set(instance_global_map)
    , m_instance_local_set(instance_local_map)
    , m_ownable_cache_ptr(ownable_cache)
{
    ALOG_TRACE("Created");
}

object_constructor_context::object_constructor_context()
    : m_path_prefix()
    , m_class_global_set()
    , m_class_local_set()
    , m_instance_global_set()
    , m_instance_local_set()
    , m_ownable_cache_ptr(nullptr)
{
    ALOG_TRACE("Created");
}

object_constructor_context::~object_constructor_context()
{
    ALOG_TRACE("Destructed");
}

utils::path
object_constructor_context::get_full_path(const utils::path& relative_path) const
{
    auto resource_path = m_path_prefix;
    resource_path.append(relative_path);

    return resource_path;
}

const utils::path&
object_constructor_context::get_full_path() const
{
    return m_path_prefix;
}

bool
object_constructor_context::add_obj(std::shared_ptr<smart_object> obj)
{
    AGEA_check(m_ownable_cache_ptr, "Should exists!");
    AGEA_check(obj, "Should exists!");

    auto& obj_ref = *obj.get();

    m_ownable_cache_ptr->add_item(std::move(obj));

    switch (m_construction_type)
    {
    case obj_construction_type::obj_construction_type__class:
        m_class_local_set.objects->add_item(obj_ref);
        m_class_local_set.map->add_item(obj_ref);
        break;
    case obj_construction_type::obj_construction_type__instance:
        m_instance_local_set.objects->add_item(obj_ref);
        m_instance_local_set.map->add_item(obj_ref);
        break;
    default:
        AGEA_never("Unsupported type type");
        break;
    }

    return true;
}

smart_object*
object_constructor_context::find_class_obj(const core::id& id)
{
    auto obj = m_class_local_set.objects ? m_class_local_set.objects->get_item(id) : nullptr;
    if (!obj)
    {
        obj = m_class_global_set.objects->get_item(id);
        if (!obj)
        {
            ALOG_LAZY_ERROR;
            return nullptr;
        }
    }

    return obj;
}

}  // namespace model
}  // namespace agea