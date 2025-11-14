#include "core/package_manager.h"

#include "core/object_constructor.h"
#include "core/object_load_context.h"
#include "core/caches/hash_cache.h"
#include "core/caches/caches_map.h"
#include "core/package.h"
#include "core/global_state.h"
#include "glue/dependency_tree.ar.h"

#include "packages/root/model/assets/texture.h"
#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/mesh.h"
#include "core/reflection/reflection_type.h"

#include <serialization/serialization.h>

#include <map>
#include <utils/agea_log.h>

namespace agea
{

namespace core
{

package_manager::package_manager()
{
}

package_manager::~package_manager()
{
}

bool
package_manager::init()
{
    std::sort(m_static_packages.begin(), m_static_packages.end(),
              [](static_package* l, static_package* r)
              { return l->get_id().str() < r->get_id().str(); });

    auto handle_dapandancy = [this](auto self, static_package* p) -> void
    {
        if (p->m_state == package_state::loaded)
        {
            return;
        }

        auto depds = get_dapendency(p->get_id());
        for (auto& d : depds)
        {
            auto sp = (static_package*)get_package(d);

            self(self, sp);
        }

        p->complete_load();
        p->m_state = package_state::loaded;
    };

    for (auto p : m_static_packages)
    {
        handle_dapandancy(handle_dapandancy, p);
    }

    return true;
}

bool
package_manager::load_package(const utils::id& id)
{
    auto itr = m_packages.find(id);
    if (itr != m_packages.end() && itr->second->get_state() == package_state::loaded)
    {
        ALOG_INFO("[{0}] already loaded", id.cstr());
        return true;
    }

    auto path = glob::glob_state().get_resource_locator()->resource(category::packages, id.str());

    ALOG_INFO("Loading package [{0}] at path [{1}]", id.cstr(), path.str());

    std::string name, extension;
    path.parse_file_name_and_ext(name, extension);

    if (name.empty() || extension.empty() || extension != "apkg")
    {
        ALOG_ERROR("Loading package failed, {0} {1}", name, extension);
        return false;
    }

    auto mapping = std::make_shared<object_mapping>();
    if (!mapping->buiild_object_mapping(path / "package.acfg"))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    auto new_package = std::make_unique<package>(AID(name), package_type::pt_dynamic);
    new_package->m_load_path = path;
    new_package->m_save_root_path = path.parent();
    new_package->init();

    new_package->get_load_context().set_prefix_path(path).set_objects_mapping(mapping);

    std::vector<root::smart_object*> loaded_obj;
    for (auto& i : mapping->m_items)
    {
        AGEA_check(i.second.is_class, "Load only package!");

        root::smart_object* obj = nullptr;
        auto rc = object_constructor::object_load(i.first, object_load_type::class_obj,
                                                  new_package->get_load_context(), obj, loaded_obj);
        if (rc != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return false;
        }

        for (auto& o : loaded_obj)
        {
            if (o->get_state() != root::smart_object_state::constructed)
            {
                o->post_load();
            }
        }

        auto mirror_id = obj->get_id();
        obj = nullptr;

        //         rc = object_constructor::mirror_object(mirror_id,
        //         new_package->get_load_context(), obj,
        //                                                loaded_obj);
        //         if (rc != result_code::ok)
        //         {
        //             ALOG_LAZY_ERROR;
        //             return false;
        //         }
        //
        //         for (auto o : loaded_obj)
        //         {
        //             if (o->get_state() != root::smart_object_state::constructed)
        //             {
        //                 o->post_load();
        //             }
        //         }
    }

    new_package->set_state(package_state::loaded);

    m_packages[id] = new_package.get();
    m_dynamic_packages.push_back(std::move(new_package));

    return true;
}

bool
package_manager::unload_package(const utils::id& id)
{
    auto p = get_package(id);

    if (!p)
    {
        ALOG_ERROR("Doesn't exist {0}", id.cstr());
        return false;
    }

    return unload_package(*p);
}

bool
package_manager::unload_package(package& p)
{
    p.unload();

    return true;
}

void
package_manager::save_package(const utils::id& id, const utils::path& root_folder)
{
    ALOG_INFO("Saving package {0}", root_folder.str());

    auto itr = m_packages.find(id);

    if (itr != m_packages.end())
    {
        return;
    }

    auto& p = *itr->second;

    if (!root_folder.exists())
    {
        std::filesystem::create_directories(root_folder.fs());
    }
    std::string name = p.get_id().str() + ".apkg";
    auto full_path = root_folder / name;

    std::map<std::string, std::string> class_pathes;

    for (auto& i : p.m_objects)
    {
        auto id = i->get_id();
        auto itr = p.m_mapping->m_items.find(id);

        if (itr == p.m_mapping->m_items.end())
        {
            ALOG_LAZY_ERROR;
            return;
        }

        auto& mapping = itr->second;
        auto full_obj_path = full_path / mapping.p;

        auto parent = full_obj_path.parent();
        if (!parent.empty())
        {
            std::filesystem::create_directories(parent.fs());
        }

        result_code result = result_code::failed;
        if (mapping.is_class)
        {
            result = object_constructor::object_save(*i, full_obj_path);
            class_pathes[i->get_id().str()] = mapping.p.str();
        }
        else
        {
            result = object_constructor::object_save(*i, full_obj_path);
        }

        if (result != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return;
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
        return;
    }

    return;
}

package*
package_manager::get_package(const utils::id& id)
{
    auto itr = m_packages.find(id);

    return itr != m_packages.end() ? (itr->second) : nullptr;
}

bool
package_manager::register_static_package(core::static_package& pkg)
{
    auto itr = m_packages.find(pkg.get_id());
    if (itr != m_packages.end())
    {
        return false;
    }

    m_packages[pkg.get_id()] = &pkg;
    m_static_packages.push_back(&pkg);

    return true;
}

}  // namespace core
}  // namespace agea