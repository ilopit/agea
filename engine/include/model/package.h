#pragma once

#include "core/agea_minimal.h"
#include "core/id.h"

#include "model/model_fwds.h"
#include "model/caches/cache_set.h"
#include "model/caches/line_cache.h"

namespace agea
{
namespace model
{

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

    const core::id&
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

    template <typename T>
    utils::path
    get_resource_path(const T& resource_path) const
    {
        return m_load_path / resource_path;
    }

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

    void
    propagate_to_global_caches();

    bool
    prepare_for_rendering();

private:
    core::id m_id;
    mutable utils::path m_load_path;
    mutable utils::path m_save_root_path;

    cache_set_ref m_class_global_set;
    cache_set_ref m_instance_global_set;

    cache_set m_class_local_set;
    cache_set m_instance_local_set;

    line_cache m_objects;

    std::unique_ptr<object_constructor_context> m_occ;
};

}  // namespace model

}  // namespace agea