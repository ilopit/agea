#include "model/level_constructor.h"

#include "model/level.h"
#include "model/mesh_object.h"
#include "model/components/mesh_component.h"
#include "model/caches/class_object_cache.h"
#include "model/caches/game_objects_cache.h"
#include "model/object_construction_context.h"
#include "model/object_constructor.h"
#include "model/package_manager.h"

#include "core/fs_locator.h"
#include "core/id.h"

#include "serialization/serialization.h"

#include <fstream>
#include <json/json.h>

#include "utils/agea_log.h"

namespace agea
{
namespace model
{

bool
level_constructor::load_level_id(level& l, const core::id& id, cache_set_ref global_cs)
{
    ALOG_INFO("Begin level loading with id {0}", id.cstr());

    auto path = glob::resource_locator::get()->resource(category::levels, id.str());

    return load_level_path(l, path, global_cs);
}

bool
level_constructor::load_level_path(level& l, const utils::path& path, cache_set_ref global_cs)
{
    ALOG_INFO("Begin level loading with path {0}", path.str());
    l.m_global_cs = global_cs;

    l.m_occ = std::make_unique<object_constructor_context>(l.m_global_cs, l.m_local_cs.get_ref(),
                                                           &l.m_objects);

    serialization::conteiner conteiner;
    if (!serialization::read_container(path, conteiner))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    {
        auto packages = conteiner["packages"];
        auto packages_count = packages.size();
        for (size_t idx = 0; idx < packages_count; ++idx)
        {
            auto id = core::id::from(packages[idx].as<std::string>());
            if (!glob::package_manager::get()->load_package(id))
            {
                ALOG_LAZY_ERROR;
                return false;
            }
            l.m_package_ids.push_back(id);
        }
    }

    auto groups_count = conteiner["instance_groups"].size();

    if (groups_count == 0)
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    for (unsigned idx = 0; idx < groups_count; ++idx)
    {
        auto json_group = conteiner["instance_groups"][idx];

        ALOG_INFO("Level : group {0}", json_group["object_class"].as<std::string>());

        auto class_id = core::id::from(json_group["object_class"].as<std::string>());
        ALOG_INFO("Level : class_id {0} instances", class_id.str());

        auto items = json_group["instances"];
        auto items_size = items.size();

        auto class_obj = l.m_global_cs.objects->get_item(class_id);

        if (!class_obj)
        {
            ALOG_LAZY_ERROR;
            return false;
        }

        for (unsigned i = 0; i < items_size; ++i)
        {
            auto item = items[i];

            auto instance = object_constructor::object_clone_create(
                class_obj->get_id(), core::id::from(item["id"].as<std::string>()), *l.m_occ);

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

    return true;
}

bool
level_constructor::save_level(level& l, const utils::path& path)
{
    serialization::conteiner conteiner;

    {
        {
            for (auto id : l.m_package_ids)
            {
                conteiner["packages"].push_back(id.str());
            }
        }

        std::unordered_map<const smart_object*, std::vector<smart_object*>> instances_groups_maping;

        auto& items = l.m_local_cs.game_objects->get_items();

        for (auto& o : items)
        {
            auto& obj = o.second;
            auto cobj = obj->get_class_obj();

            if (!obj->as<game_object>())
            {
                continue;
            }

            instances_groups_maping[cobj].push_back(obj);
        }

        serialization::conteiner instances_groups;

        for (auto& instance_group : instances_groups_maping)
        {
            instances_groups["object_class"] = instance_group.first->get_id().str();

            serialization::conteiner instances_conteiner;

            for (auto& instance : instance_group.second)
            {
                serialization::conteiner intance;
                intance["id"] = instance->get_id().str();

                auto& base_obj = *instance_group.first;
                auto& obj_to_diff = *instance;

                std::vector<reflection::property*> diff;
                object_constructor::diff_object_properties(base_obj, obj_to_diff, diff);

                reflection::serialize_context ctx;
                ctx.obj = instance;
                ctx.sc = &intance;
                for (auto p : diff)
                {
                    ctx.p = p;
                    p->serialization_handler(ctx);
                }

                instances_groups["instances"].push_back(intance);
            }
        }

        conteiner["instance_groups"].push_back(instances_groups);
    }

    if (!serialization::write_container(path, conteiner))
    {
        return false;
    }
    return true;
}
}  // namespace model

}  // namespace agea
