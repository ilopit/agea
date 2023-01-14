#include "model/level_constructor.h"

#include "model/level.h"
#include "model/mesh_object.h"
#include "model/components/mesh_component.h"
#include "model/object_construction_context.h"
#include "model/object_constructor.h"
#include "model/package_manager.h"
#include "model/caches/cache_set.h"
#include "model/caches/hash_cache.h"
#include "model/caches/caches_map.h"

#include <resource_locator/resource_locator.h>

#include <serialization/serialization.h>

#include <fstream>

#include <utils/agea_log.h>

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
                                 const utils::id& id,
                                 cache_set* global_class_cs,
                                 cache_set* global_instances_cs)
{
    ALOG_INFO("Begin level loading with id {0}", id.cstr());

    auto path = glob::resource_locator::get()->resource(category::levels, id.str());

    return load_level_path(l, path, global_class_cs, global_instances_cs);
}

bool
level_constructor::load_level_path(level& l,
                                   const utils::path& path,
                                   cache_set* global_class_cs,
                                   cache_set* global_instances_cs)
{
    ALOG_INFO("Begin level loading with path {0}", path.str());

    l.m_global_class_object_cs = global_class_cs;
    l.m_global_object_cs = global_instances_cs;
    l.set_load_path(path);
    l.set_save_root_path(path.parent());

    std::string name, extension;
    path.parse_file_name_and_ext(name, extension);

    if (name.empty() || extension.empty() || extension != "alvl")
    {
        ALOG_ERROR("Loading level failed, {0} {1}", name, extension);
        return false;
    }
    l.m_id = AID(name);

    auto root_path = path / "root.cfg";

    serialization::conteiner conteiner;
    if (!serialization::read_container(root_path, conteiner))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    if (!l.m_mapping.buiild_object_mapping(root_path))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    l.m_occ = std::make_unique<object_constructor_context>();
    l.m_occ->set_prefix_path(utils::path{})
        .set_class_global_set(l.m_global_class_object_cs)
        .set_instance_global_set(l.m_global_object_cs)
        .set_class_local_set(nullptr)
        .set_instance_local_set(&l.m_local_cs)
        .set_ownable_cache(&l.m_objects)
        .set_objects_mapping(l.m_mapping.m_items)
        .set_prefix_path(path);

    {
        auto packages = conteiner["packages"];
        auto packages_count = packages.size();
        for (size_t idx = 0; idx < packages_count; ++idx)
        {
            auto id = AID(packages[idx].as<std::string>());
            if (!glob::package_manager::get()->load_package(id))
            {
                ALOG_LAZY_ERROR;
                return false;
            }
            l.m_package_ids.push_back(id);
        }
    }

    for (auto& i : l.m_mapping.m_items)
    {
        auto& mapping = i.second;

        l.m_occ->set_construction_type(i.second.first ? obj_construction_type::class_obj
                                                      : obj_construction_type::instance_obj);
        object_constructor::object_load(i.first, *l.m_occ);
        l.m_occ->set_construction_type(obj_construction_type::nav);
    }

    for (auto& o : l.m_objects)
    {
        o->META_post_construct();
    }

    for (auto& o : l.m_objects)
    {
        if (auto a = o->as<game_object>())
        {
            l.m_tickable_objects.emplace_back(a);
        }
    }

    size_t idx = 0;
    for (auto& c : l.m_local_cs.map->get_items())
    {
        for (auto& i : c.second->get_items())
        {
            global_instances_cs->map->get_cache(c.first)->add_item(*i.second);
            ++idx;
        }
    }
    ALOG_INFO("Propageted {0} object instances from {1} to global cache", idx, name);

    return true;
}

bool
level_constructor::save_level(level& l, const utils::path& path)
{
    l.set_save_root_path(path);
    std::string name = l.get_id().str() + ".alvl";
    auto full_path = path / name;

    if (!full_path.exists())
    {
        std::filesystem::create_directories(full_path.fs());
    }

    serialization::conteiner conteiner;
    for (auto& id : l.m_package_ids)
    {
        conteiner["packages"].push_back(id.str());
    }
    auto root_path = full_path / "root.cfg";
    if (!serialization::write_container(root_path, conteiner))
    {
        return false;
    }

    auto instace_path = full_path;

    return true;
}

}  // namespace model
}  // namespace agea
