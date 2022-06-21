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

namespace agea
{
namespace model
{

object_constructor_context::object_constructor_context(
    cache_set_ref global_map,
    cache_set_ref local_map,
    std::vector<std::shared_ptr<smart_object>>* local_objcs)
    : m_path_prefix()
    , m_global_set(global_map)
    , m_local_set(local_map)
    , m_local_objecs(local_objcs)
{
    ALOG_TRACE("Created");
}

object_constructor_context::object_constructor_context()
    : m_global_set()
    , m_local_set()
    , m_local_objecs(nullptr)
{
}

object_constructor_context::~object_constructor_context()
{
    ALOG_TRACE("Destructed");
}

bool
object_constructor_context::propagate_to_io_cache()
{
    return true;
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
object_constructor_context::propagate_to_co_cache()
{
    return true;
}

bool
object_constructor_context::add_obj(std::shared_ptr<smart_object> obj)
{
    AGEA_check(m_local_objecs, "Should exists!");
    AGEA_check(obj, "Should exists!");

    auto& obj_ref = *obj.get();

    m_local_objecs->push_back(std::move(obj));

    m_local_set.objects->add_item(obj_ref);
    m_local_set.map->add_item(obj_ref);

    return true;
}

}  // namespace model
}  // namespace agea