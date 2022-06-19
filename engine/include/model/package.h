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
    package();
    ~package();

    package(package&&) noexcept;
    package&
    operator=(package&&) noexcept;

    static bool
    load_package(const std::string& path, package& p, cache_set_ref global_cs);

    static bool
    save_package(const std::string& path, const package& p);

    static bool
    load_package_conteiners(architype id, package& p);

    const std::string&
    get_id() const
    {
        return m_id;
    }

    const std::string&
    get_path() const
    {
        return m_path;
    }

    std::string
    get_resource_path(const std::string& path) const
    {
        return m_path + "/" + path;
    }

    void
    set_path(const std::string& path) const
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
    mutable std::string m_path;

    cache_set m_local_cs;
    cache_set_ref m_global_cs;
    std::vector<std::shared_ptr<smart_object>> m_objects;

    std::unique_ptr<object_constructor_context> m_occ;
};

}  // namespace model

}  // namespace agea