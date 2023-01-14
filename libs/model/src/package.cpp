#include "model/package.h"

#include "model/object_constructor.h"
#include "model/object_construction_context.h"
#include "model/caches/hash_cache.h"
#include "model/caches/caches_map.h"

#include "model/assets/texture.h"
#include "model/assets/material.h"
#include "model/assets/mesh.h"

#include <utils/agea_log.h>

#include <serialization/serialization.h>

#include <map>
#include <filesystem>

namespace agea
{
namespace model
{

package::package(package&&) noexcept = default;
package&
package::operator=(package&&) noexcept = default;

package::package()
    : m_occ(std::make_unique<object_constructor_context>())
{
}

package::~package()
{
}

bool
package::load_package(const utils::path& path,
                      package& p,
                      cache_set* class_global_set,
                      cache_set* instance_global_set)
{
    ALOG_INFO("Loading package {0}", path.str());

    std::string name, extension;
    path.parse_file_name_and_ext(name, extension);

    if (name.empty() || extension.empty() || extension != "apkg")
    {
        ALOG_ERROR("Loading package failed, {0} {1}", name, extension);
        return false;
    }

    p.m_class_global_set = class_global_set;
    p.m_instance_global_set = instance_global_set;
    p.m_load_path = path;
    p.m_save_root_path = path.parent();
    p.m_id = AID(name);

    if (!p.m_mapping.buiild_object_mapping(path / "package.acfg"))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    p.m_occ->set_prefix_path(p.m_load_path)
        .set_class_global_set(p.m_class_global_set)
        .set_instance_global_set(p.m_instance_global_set)
        .set_class_local_set(&p.m_class_local_set)
        .set_instance_local_set(&p.m_instance_local_set)
        .set_objects_mapping(p.m_mapping.m_items)
        .set_ownable_cache(&p.m_objects);

    for (auto& i : p.m_mapping.m_items)
    {
        p.m_occ->set_construction_type(i.second.first ? obj_construction_type::class_obj
                                                      : obj_construction_type::instance_obj);
        object_constructor::object_load(i.first, *p.m_occ);
        p.m_occ->set_construction_type(obj_construction_type::nav);
    }

    for (auto& o : p.m_objects)
    {
        o->set_package(&p);
    }

    p.m_state = package_state::loaded;

    return true;
}

bool
package::save_package(const utils::path& root_folder, const package& p)
{
    ALOG_INFO("Sabing package {0}", root_folder.str());

    p.set_save_root_path(root_folder);

    if (!root_folder.exists())
    {
        ALOG_INFO("There is no [{0}] conainer in [{1}] package!", p.get_id().cstr());
        std::filesystem::create_directories(root_folder.fs());
    }
    std::string name = p.get_id().str() + ".apkg";
    auto full_path = root_folder / name;

    std::map<std::string, std::string> class_pathes;

    for (auto& i : p.m_objects)
    {
        auto id = i->get_id();
        auto itr = p.m_mapping.m_items.find(id);

        if (itr == p.m_mapping.m_items.end())
        {
            ALOG_LAZY_ERROR;
            return false;
        }

        auto& mapping = itr->second;
        auto full_obj_path = full_path / mapping.second;

        auto parent = full_obj_path.parent();
        if (!parent.empty())
        {
            std::filesystem::create_directories(parent.fs());
        }

        bool result = false;
        if (mapping.first)
        {
            result = object_constructor::object_save(*i, full_obj_path);
            class_pathes[i->get_id().str()] = mapping.second.str();
        }
        else
        {
            result = object_constructor::object_save(*i, full_obj_path);
        }

        if (!result)
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    auto meta_file = full_path / "package.cfg";
    serialization::conteiner meta_conteiner;

    int i = 0;
    for (auto& c : class_pathes)
    {
        meta_conteiner["class_obj_mapping"][i++][c.first] = c.second;
    }

    if (!serialization::write_container(meta_file, meta_conteiner))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    return true;
}

utils::path
package::get_relative_path(const utils::path& p) const
{
    return p.relative(m_save_root_path);
}

void
package::propagate_to_global_caches()
{
    ALOG_INFO("Propagate to global [{0}]", m_id.cstr());

    size_t size = m_objects.get_size();
    for (size_t i = 0; i < size; ++i)
    {
        auto& e = m_objects[i];
        AGEA_check(e->has_state(smart_object_internal_state::class_obj), "Never happens");

        ALOG_INFO("Mirroring [{0}]", e->get_id().cstr());

        m_occ->set_construction_type(obj_construction_type::mirror_obj);
        auto o = object_constructor::object_clone_create(e->get_id(), e->get_id(), *m_occ);
        m_occ->set_construction_type(obj_construction_type::nav);

        if (o)
        {
            m_package_instances.emplace_back(o);
        }
    }

    size_t count = 0;
    for (auto& c : m_class_local_set.map->get_items())
    {
        for (auto& i : c.second->get_items())
        {
            m_class_global_set->map->get_cache(c.first)->add_item(*i.second);
            ++count;
        }
    }

    for (auto& c : m_instance_local_set.map->get_items())
    {
        for (auto& i : c.second->get_items())
        {
            m_instance_global_set->map->get_cache(c.first)->add_item(*i.second);
            ++count;
        }
    }

    for (auto& c : m_objects)
    {
        c->META_post_construct();
    }

    ALOG_INFO("Propageted {0} object instances from {1} to global cache", count, m_id.cstr());
}

}  // namespace model
}  // namespace agea