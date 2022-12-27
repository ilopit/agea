#pragma once

#include "model/model_minimal.h"

#include "model/model_fwds.h"
#include "model/caches/cache_set.h"
#include "model/caches/line_cache.h"

#include "object_mapping.h"

namespace agea
{
namespace model
{
enum package_state
{
    package_state__unloaded,
    package_state__loaded,
    package_state__render_loaded
};

class package
{
public:
    static bool
    load_package(const utils::path& root_folder,
                 package& p,
                 cache_set_ref class_global_set,
                 cache_set_ref instance_global_set);

    static bool
    save_package(const utils::path& root_folder, const package& p);

    package();
    ~package();

    package(package&&) noexcept;
    package&
    operator=(package&&) noexcept;

    const utils::id&
    get_id() const
    {
        return m_id;
    }

    const utils::path&
    get_load_path() const
    {
        return m_load_path;
    }

    const utils::path&
    get_save_path() const
    {
        return m_save_root_path;
    }

    utils::path
    get_relative_path(const utils::path& p) const;

    void
    set_save_root_path(const utils::path& path) const
    {
        m_save_root_path = path;
    }

    void
    set_load_path(const utils::path& path) const
    {
        m_load_path = path;
    }

    cache_set&
    get_cache()
    {
        return m_instance_local_set;
    }

    cache_set&
    get_class_cache()
    {
        return m_class_local_set;
    }

    void
    propagate_to_global_caches();

    package_state
    get_state()
    {
        return m_state;
    }

    void
    set_state(package_state v)
    {
        m_state = v;
    }

    line_cache<std::shared_ptr<smart_object>>&
    get_objects()
    {
        return m_objects;
    }

private:
    utils::id m_id;
    mutable utils::path m_load_path;
    mutable utils::path m_save_root_path;

    package_state m_state = package_state__unloaded;

    cache_set_ref m_class_global_set;
    cache_set_ref m_instance_global_set;

    cache_set m_class_local_set;
    cache_set m_instance_local_set;

    line_cache<std::shared_ptr<smart_object>> m_objects;

    object_mapping m_mapping;

    std::unique_ptr<object_constructor_context> m_occ;
};

}  // namespace model

}  // namespace agea