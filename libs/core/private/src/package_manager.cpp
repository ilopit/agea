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
#include <global_state/global_state.h>

#include <serialization/serialization.h>

#include <deque>
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
    deinit();
}

static_package_context::static_package_context()
{
}

static_package_context::~static_package_context()
{
}

bool
package_manager::init()
{
    // Collect all static packages
    std::vector<package*> packages;
    for (auto& p : m_static_packages)
    {
        packages.push_back(p.second.pkg.get());
    }

    // Topological sort using Kahn's algorithm
    std::unordered_map<utils::id, int> in_degree;
    std::unordered_map<utils::id, std::vector<utils::id>> dependents;

    // Initialize in-degree for all packages
    for (auto* pkg : packages)
    {
        in_degree[pkg->get_id()] = 0;
    }

    // Build dependency graph and calculate in-degrees
    for (auto* pkg : packages)
    {
        auto deps = get_dapendency(pkg->get_id());
        in_degree[pkg->get_id()] = static_cast<int>(deps.size());

        for (const auto& dep_id : deps)
        {
            dependents[dep_id].push_back(pkg->get_id());
        }
    }

    // Find all packages with no dependencies (in_degree == 0)

    std::deque<package*> queue;

    for (auto* pkg : packages)
    {
        if (in_degree[pkg->get_id()] == 0)
        {
            queue.push_back(pkg);
        }
    }

    // Process packages in topological order
    while (!queue.empty())
    {
        auto* pkg = queue.front();
        queue.pop_front();
        m_sorted_static_packages.push_back(pkg);

        // Reduce in-degree for all dependents
        for (const auto& dep_id : dependents[pkg->get_id()])
        {
            in_degree[dep_id]--;
            if (in_degree[dep_id] == 0)
            {
                auto* dep_pkg = get_package(dep_id);
                if (dep_pkg)
                {
                    queue.push_back(dep_pkg);
                }
            }
        }
    }

    // Load packages in topological order
    for (auto* pkg : m_sorted_static_packages)
    {
        ALOG_INFO("Loading {}", pkg->get_id().str());
        pkg->complete_load();
    }

    return true;
}

bool
package_manager::deinit()
{
    for (auto itr = m_sorted_static_packages.rbegin(); itr != m_sorted_static_packages.rend();
         ++itr)
    {
        (*itr)->unload();
    }

    return true;
}

package&
package_manager::load_static_package(const utils::id& package_id)
{
    auto& ctx = m_static_packages[package_id];
    ctx.pkg = ctx.loader();
    m_packages[ctx.pkg->get_id()] = ctx.pkg.get();

    return *ctx.pkg.get();
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

    auto new_package = std::make_unique<package>(AID(name));
    new_package->m_load_path = path;
    new_package->m_save_root_path = path.parent();
    new_package->init();

    new_package->get_load_context().set_prefix_path(path).set_objects_mapping(mapping);

    std::vector<root::smart_object*> loaded_obj;
    for (auto& i : mapping->m_items)
    {
        AGEA_check(i.second.is_class, "Load only package!");

        auto result = object_constructor::object_load(i.first, object_load_type::class_obj,
                                                      new_package->get_load_context(), loaded_obj);
        if (!result)
        {
            ALOG_LAZY_ERROR;
            return false;
        }
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

    if (itr == m_packages.end())
    {
        ALOG_ERROR("Package not found: {0}", id.cstr());
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
package_manager::register_package(core::package& pkg)
{
    auto itr = m_packages.find(pkg.get_id());
    if (itr != m_packages.end())
    {
        return false;
    }

    m_packages[pkg.get_id()] = &pkg;
    //  m_static_packages.push_back(&pkg);

    return true;
}

}  // namespace core
}  // namespace agea