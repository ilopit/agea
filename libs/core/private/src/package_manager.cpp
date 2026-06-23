#include "core/package_manager.h"

#include "core/object_constructor.h"
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

package_manager::package_manager() = default;

package_manager::~package_manager()
{
    deinit();
}

static_package_context::static_package_context() = default;

static_package_context::~static_package_context() = default;

bool
package_manager::init()
{
    // Collect all static packages
    std::vector<package*> packages;
    packages.reserve(m_static_packages.size());
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

    return *ctx.pkg;
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

    // Read dependencies from package.acfg before constructing the package so we
    // can transitively load them first. Static packages (root/base/...) are
    // already loaded by package_manager::init(); load_package() on them
    // short-circuits above.
    std::vector<utils::id> runtime_deps;
    {
        auto acfg_rid = vfs_paths::package_root(id) / "package.acfg";
        serialization::container meta;
        if (serialization::read_container(acfg_rid, meta))
        {
            auto deps_node = meta["dependencies"];
            if (deps_node && deps_node.IsSequence())
            {
                for (auto&& item : deps_node)
                {
                    runtime_deps.push_back(AID(item.as<std::string>()));
                }
            }
        }
    }

    for (const auto& dep_id : runtime_deps)
    {
        if (!load_package(dep_id))
        {
            ALOG_ERROR("Failed to load dep [{0}] of package [{1}]", dep_id.cstr(), id.cstr());
            return false;
        }
    }

    auto new_package = std::make_unique<package>(AID(id.str()));
    if (!new_package->init())
    {
        ALOG_ERROR("Failed to init package {}", id.cstr());
        return false;
    }
    new_package->set_runtime_dependencies(std::move(runtime_deps));

    {
        auto& vfs = glob::glob_state().getr_vfs();
        bool load_ok = true;
        object_constructor ctor(&new_package->get_load_context());
        vfs.enumerate_objects(
            new_package->get_vfs_root(),
            [&](std::string_view name, const vfs::rid&) -> bool
            {
                auto result = ctor.load_obj(AID(std::string(name)));
                if (!result)
                {
                    load_ok = false;
                    return false;
                }
                return true;
            },
            new_package->m_backend);

        if (!load_ok)
        {
            return false;
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

    auto& vfs = glob::glob_state().getr_vfs();
    object_constructor ctor(&p.get_load_context());
    for (auto& i : p.m_objects)
    {
        auto obj_id = i->get_id();
        auto found = vfs.find_object(p.get_vfs_root(), obj_id.str());
        if (!found)
        {
            ALOG_LAZY_ERROR;
            return false;
        }

        auto rc = ctor.save_obj(*i);
        class_paths[obj_id.str()] = std::string(found->relative());

        if (rc != result_code::ok)
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
