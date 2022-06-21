#pragma once

#include "core/agea_minimal.h"

#include "model/model_fwds.h"

#include "model/caches/cache_set.h"

namespace agea
{
namespace model
{

class package
{
public:
    static bool
    load_package(const utils::path& path, package& p, cache_set_ref global_cs);

    static bool
    save_package(const utils::path& path, const package& p);

    static bool
    load_package_conteiners(architype id, package& p);

    package();
    ~package();

    package(package&&) noexcept;
    package&
    operator=(package&&) noexcept;

    const std::string&
    get_id() const
    {
        return m_id;
    }

    const utils::path&
    get_path() const
    {
        return m_path;
    }

    template <typename T>
    utils::path
    get_resource_path(const T& resource_path) const
    {
        auto path = m_path;
        path.append(resource_path);

        return path;
    }

    void
    set_path(const utils::path& path) const
    {
        m_path = path;
    }

    cache_set&
    get_cache()
    {
        return m_local_cs;
    }

    void
    propagate_to_global_caches();

private:
    std::string m_id;
    mutable utils::path m_path;

    cache_set m_local_cs;
    cache_set_ref m_global_cs;

    std::vector<std::shared_ptr<smart_object>> m_objects;

    std::unique_ptr<object_constructor_context> m_occ;
};

}  // namespace model

}  // namespace agea