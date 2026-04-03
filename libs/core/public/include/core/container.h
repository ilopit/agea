#pragma once

#include "core/model_minimal.h"
#include "core/model_fwds.h"

#include "core/caches/cache_set.h"
#include "core/caches/line_cache.h"

#include "core/object_constructor.h"

#include <utils/id.h>
#include <utils/path.h>
#include <vfs/rid.h>

namespace kryga
{
namespace core
{

class object_load_context;

// Canonical VFS path builders — enforce that packages/levels live under data://
namespace vfs_paths
{

inline constexpr std::string_view k_mount = "data";
inline constexpr std::string_view k_packages_prefix = "packages/";
inline constexpr std::string_view k_levels_prefix = "levels/";

inline vfs::rid
package_root(const utils::id& id)
{
    return vfs::rid(k_mount, std::string(k_packages_prefix) + id.str() + ".apkg");
}

inline vfs::rid
level_root(const utils::id& id)
{
    return vfs::rid(k_mount, std::string(k_levels_prefix) + id.str() + ".alvl");
}

inline bool
is_valid_package_root(const vfs::rid& r)
{
    return r.mount_point() == k_mount &&
           r.relative().substr(0, k_packages_prefix.size()) == k_packages_prefix;
}

inline bool
is_valid_level_root(const vfs::rid& r)
{
    return r.mount_point() == k_mount &&
           r.relative().substr(0, k_levels_prefix.size()) == k_levels_prefix;
}

}  // namespace vfs_paths

class container
{
public:
    container(const utils::id& id);
    ~container();

    container(container&&) noexcept;
    container&
    operator=(container&&) noexcept;

    friend class package_manager;

    const utils::id&
    get_id() const
    {
        return m_id;
    }

    const vfs::rid&
    get_vfs_root() const
    {
        return m_vfs_root;
    }

    void
    set_vfs_root(const vfs::rid& r);

    const utils::path&
    get_save_path() const
    {
        return m_save_root_path;
    }

    void
    set_save_root_path(const utils::path& p)
    {
        m_save_root_path = p;
    }

    utils::path
    get_relative_path(const utils::path& p) const;

    cache_set&
    get_local_cache()
    {
        return m_instance_local_cs;
    }

    object_load_context&
    get_load_context() const
    {
        return *m_occ.get();
    }

    static void
    unregister_in_global_cache(cache_set& local,
                               cache_set& global,
                               const utils::id& id,
                               const char* extra);

    void
    unload();

    const line_cache<std::shared_ptr<root::smart_object>>
    get_objects() const
    {
        return m_objects;
    }

    void
    set_occ(std::unique_ptr<object_load_context> occ);

protected:
    utils::id m_id;
    vfs::rid m_vfs_root;
    utils::path m_save_root_path;

    cache_set m_instance_local_cs;

    line_cache<std::shared_ptr<root::smart_object>> m_objects;
    std::unique_ptr<object_load_context> m_occ;
};

}  // namespace core

}  // namespace kryga
