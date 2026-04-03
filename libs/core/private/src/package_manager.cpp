#include "core/package_manager.h"

#include "core/object_constructor.h"
#include "core/object_load_context.h"
#include "core/caches/hash_cache.h"
#include "core/caches/caches_map.h"
#include "core/package.h"
#include "glue/dependency_tree.ar.h"

#include "packages/root/model/assets/texture.h"
#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/mesh.h"
#include "core/reflection/reflection_type.h"
#include <global_state/global_state.h>
#include <vfs/vfs.h>
#include <vfs/rid.h>

#include <serialization/serialization.h>

#include <deque>
#include <map>
#include <utils/kryga_log.h>

namespace kryga
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
        auto deps = get_dependency(pkg->get_id());
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

    auto new_package = std::make_unique<package>(AID(id.str()));
    if (!new_package->init())
    {
        ALOG_ERROR("Failed to init package {}", id.cstr());
        return false;
    }

    {
        auto& mapping = new_package->get_load_context().get_objects_mapping();
        object_constructor ctor(&new_package->get_load_context());
        for (auto& i : mapping.m_items)
        {
            KRG_check(i.second.is_class, "Load only package!");

            auto result = ctor.load_package_obj(i.first);
            if (!result)
            {
                ALOG_LAZY_ERROR;
                return false;
            }
        }
        new_package->get_load_context().reset_loaded_objects();
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

bool
package_manager::save_package(const utils::id& id, const utils::path& root_folder)
{
    ALOG_INFO("Saving package {0}", root_folder.str());

    auto itr = m_packages.find(id);

    if (itr == m_packages.end())
    {
        ALOG_ERROR("Package not found: {0}", id.cstr());
        return false;
    }

    auto& p = *itr->second;

    if (!root_folder.exists())
    {
        std::filesystem::create_directories(root_folder.fs());
    }
    std::string name = p.get_id().str() + ".apkg";
    auto full_path = root_folder / name;

    std::map<std::string, std::string> class_paths;

    for (auto& i : p.m_objects)
    {
        auto id = i->get_id();
        auto itr = p.m_mapping->m_items.find(id);

        if (itr == p.m_mapping->m_items.end())
        {
            ALOG_LAZY_ERROR;
            return false;
        }

        auto& mapping = itr->second;
        auto full_obj_path = full_path / mapping.p;

        auto parent = full_obj_path.parent();
        if (!parent.empty())
        {
            std::filesystem::create_directories(parent.fs());
        }

        auto result = object_constructor::object_save(*i, full_obj_path);
        if (mapping.is_class)
        {
            class_paths[i->get_id().str()] = mapping.p.str();
        }

        if (result != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    auto meta_file = full_path / "package.acfg";
    serialization::container meta_container;

    int i = 0;
    for (auto& c : class_paths)
    {
        meta_container["class_obj_mapping"][i++][c.first] = c.second;
    }

    if (!serialization::write_container(meta_file, meta_container))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    return true;
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
}  // namespace kryga
