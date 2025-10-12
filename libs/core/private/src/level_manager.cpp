#include "core/level_manager.h"

#include "core/level.h"
#include "core/object_load_context.h"
#include "core/object_constructor.h"
#include "core/package_manager.h"
#include "core/caches/cache_set.h"
#include "core/caches/hash_cache.h"
#include "core/caches/caches_map.h"
#include "core/global_state.h"

#include <packages/root/model/mesh_object.h>
#include <packages/root/model/components/mesh_component.h>

#include <utils/agea_log.h>

#include <resource_locator/resource_locator.h>

#include <serialization/serialization.h>

#include <fstream>

namespace agea
{
namespace core
{

level*
level_manager::load_level(const utils::id& id,
                          cache_set* global_class_cs,
                          cache_set* global_instances_cs)
{
    ALOG_INFO("Begin level loading with id {0}", id.cstr());

    auto& l = m_levels[id];

    if (l)
    {
        if (l->get_state() == level_state::loaded)
        {
            ALOG_INFO("[{0}] already loaded", id.cstr());
            return l.get();
        }
    }
    else
    {
        l = std::make_unique<level>(id, global_class_cs, global_instances_cs);
    }

    auto level_id = id.str() + ".alvl";

    auto path = glob::resource_locator::get()->resource(category::levels, level_id);

    return load_level_path(*l, path, global_class_cs, global_instances_cs);
}

level*
level_manager::load_level_path(level& l,
                               const utils::path& path,
                               cache_set* global_class_cs,
                               cache_set* global_instances_cs)
{
    ALOG_INFO("Begin level loading with path {0}", path.str());

    l.set_load_path(path);
    l.set_save_root_path(path.parent());
    // l.init_global_cache_reference(global_class_cs, global_instances_cs);

    std::string name, extension;
    path.parse_file_name_and_ext(name, extension);

    if (name.empty() || extension.empty() || extension != "alvl")
    {
        ALOG_ERROR("Loading level failed, {0} {1}", name, extension);
        return nullptr;
    }

    AGEA_check(l.m_id == AID(name), "Should be same");

    auto root_path = path / "root.cfg";

    serialization::conteiner conteiner;
    if (!serialization::read_container(root_path, conteiner))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    if (!l.m_mapping->buiild_object_mapping(root_path))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    };

    {
        auto packages = conteiner["packages"];
        auto packages_count = packages.size();
        for (size_t idx = 0; idx < packages_count; ++idx)
        {
            auto id = AID(packages[idx].as<std::string>());
            if (!glob::state::getr().get_pm()->load_package(id))
            {
                ALOG_LAZY_ERROR;
                return nullptr;
            }
            l.m_package_ids.push_back(id);
        }
    }

    std::vector<root::smart_object*> loaded_obj;

    for (auto& i : l.m_mapping->m_items)
    {
        root::smart_object* obj = nullptr;
        auto rc = object_constructor::object_load(i.first, object_load_type::instance_obj, *l.m_occ,
                                                  obj, loaded_obj);

        if (rc != result_code::ok)
        {
            return nullptr;
        }

        for (auto o : loaded_obj)
        {
            o->post_load();
        }

        if (auto go = obj->as<root::game_object>())
        {
            l.add_to_dirty_render_queue(go->get_root_component());
        }
    }
    l.m_state = level_state::loaded;

    return &l;
}

void
level_manager::unload_level(level& l)
{
    l.drop_pending_updates();
    l.unregister_objects();
    l.unload();
}

bool
level_manager::save_level(level& l, const utils::path& path)
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

    return true;
}

}  // namespace core
}  // namespace agea
