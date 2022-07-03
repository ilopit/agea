#include "model/level_constructor.h"

#include "model/level.h"
#include "model/mesh_object.h"
#include "model/components/mesh_component.h"
#include "model/caches/class_object_cache.h"
#include "model/caches/game_objects_cache.h"
#include "model/object_construction_context.h"
#include "model/object_constructor.h"
#include "model/package_manager.h"
#include "model/conteiner_loader.h"

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

namespace
{

const std::vector<architype> k_enums_to_handle{architype::component, architype::game_object};
}

bool
level_constructor::load_level_id(level& l,
                                 const core::id& id,
                                 cache_set_ref global_class_cs,
                                 cache_set_ref global_instances_cs)
{
    ALOG_INFO("Begin level loading with id {0}", id.cstr());

    auto path = glob::resource_locator::get()->resource(category::levels, id.str());

    return load_level_path(l, path, global_class_cs, global_instances_cs);
}

bool
level_constructor::load_level_path(level& l,
                                   const utils::path& path,
                                   cache_set_ref global_class_cs,
                                   cache_set_ref global_instances_cs)
{
    ALOG_INFO("Begin level loading with path {0}", path.str());
    l.m_global_class_object_cs = global_class_cs;
    l.m_global_object_cs = global_instances_cs;

    l.m_occ = std::make_unique<object_constructor_context>(l.m_global_class_object_cs,
                                                           cache_set_ref{}, l.m_global_object_cs,
                                                           l.m_local_cs.get_ref(), &l.m_objects);

    auto root_path = path;
    root_path.append("root.cfg");
    serialization::conteiner conteiner;
    if (!serialization::read_container(root_path, conteiner))
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

    auto instace_path = path;
    instace_path.append("instance");

    for (auto id : k_enums_to_handle)
    {
        if (!conteiner_loader::load_objects_conteiners(id, object_constructor::instance_object_load,
                                                       instace_path, *l.m_occ))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    for (auto o : l.m_objects.get_items())
    {
        o->META_post_construct();
    }

    return true;
}

bool
level_constructor::save_level(level& l, const utils::path& path)
{
    serialization::conteiner conteiner;

    if (!path.exists())
    {
        std::filesystem::create_directories(path.fs());
    }

    for (auto& id : l.m_package_ids)
    {
        conteiner["packages"].push_back(id.str());
    }
    auto root_path = path;
    root_path.append("root.cfg");
    if (!serialization::write_container(root_path, conteiner))
    {
        return false;
    }

    auto instace_path = path;
    instace_path.append("instance");

    for (auto id : k_enums_to_handle)
    {
        if (!conteiner_loader::save_objects_conteiners(id, object_constructor::instance_object_save,
                                                       instace_path, l.m_local_cs.get_ref()))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    return true;
}

}  // namespace model
}  // namespace agea
