#include "model/level_constructor.h"

#include "model/level.h"
#include "model/mesh_object.h"
#include "model/components/mesh_component.h"
#include "model/caches/class_object_cache.h"
#include "model/caches/objects_cache.h"
#include "model/object_construction_context.h"
#include "model/object_constructor.h"
#include "core/fs_locator.h"

#include "serialization/serialization.h"

#include <fstream>
#include <json/json.h>

#include "utils/agea_log.h"

namespace agea
{
namespace model
{
namespace level_constructor
{

bool
load_level_id(level& l, const std::string& id)
{
    ALOG_INFO("Begin level loading with id {0}", id);

    auto path = glob::resource_locator::get()->resource(category::levels, id);

    return load_level_path(l, path);
}

bool
load_level_path(level& l, const std::string& path)
{
    ALOG_INFO("Begin level loading with path {0}", path);

    YAML::Node json = YAML::LoadFile(path);

    {
        auto objects_count = json["objecs"].size();

        if (objects_count == 0)
        {
            ALOG_LAZY_ERROR;
            return false;
        }

        for (unsigned idx = 0; idx < objects_count; ++idx)
        {
            auto obj_path = json["objecs"][idx].as<std::string>();

            auto class_obj_path = glob::resource_locator::get()->resource(category::all, obj_path);

            if (!object_constructor::class_object_load(class_obj_path, *l.m_occ))
            {
                ALOG_LAZY_ERROR;
                return false;
            }
        }
    }

    auto groups_count = json["groups"].size();

    if (groups_count == 0)
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    for (unsigned idx = 0; idx < groups_count; ++idx)
    {
        auto json_group = json["groups"][idx];

        ALOG_INFO("Level : group {0}", json_group["name"].as<std::string>());

        auto class_id = json_group["object_class"].as<std::string>();
        ALOG_INFO("Level : class_id {0} instances", class_id);

        auto items = json_group["instances"];
        auto items_size = items.size();

        auto class_obj = l.m_occ->class_obj_cache->get(class_id);

        if (!class_obj)
        {
            ALOG_LAZY_ERROR;
            return false;
        }

        for (unsigned i = 0; i < items_size; ++i)
        {
            auto item = items[i];

            auto instance = object_constructor::object_clone_create(
                class_obj->get_id(), item["id"].as<std::string>(), *l.m_occ);

            if (!instance)
            {
                ALOG_LAZY_ERROR;
                return false;
            }

            if (!object_constructor::update_object_properties(*instance, item))
            {
                ALOG_LAZY_ERROR;
                return false;
            }

            if (!l.m_occ->propagate_to_io_cache())
            {
                ALOG_LAZY_ERROR;
                return false;
            }

            if (!instance->META_post_construct())
            {
                ALOG_LAZY_ERROR;
                return false;
            }
        }
    }
    fill_level_caches(l);

    ALOG_INFO("Stats: CO items {0}, IO items {1}, Game objects {2}",
              l.m_occ->class_obj_cache->size(), l.m_occ->instance_obj_cache->size(),
              l.m_objects.size());

    return true;
}

bool
fill_level_caches(level& l)
{
    auto& occ = l.m_occ;
    for (auto& o : occ->instance_obj_cache->items())
    {
        auto game_obj = o.second->as<game_object>();
        if (game_obj)
        {
            l.m_objects.push_back(game_obj);
        }
    }

    return true;
}

}  // namespace level_constructor
}  // namespace model
}  // namespace agea
