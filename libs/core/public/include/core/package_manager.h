#pragma once

#include "core/model_minimal.h"

#include "core/model_fwds.h"

#include <utils/singleton_instance.h>

#include <unordered_set>

namespace agea
{
namespace core
{
class package_manager
{
public:
    package_manager();
    ~package_manager();

    bool
    init();

    bool
    load_package(const utils::id& id);

    bool
    unload_package(const utils::id& id);

    bool
    unload_package(package& p);

    void
    save_package(const utils::id& id, const utils::path& root_folder);

    package*
    get_package(const utils::id& id);

    bool
    register_static_package(static_package& pkg);

    std::unordered_map<utils::id, package*>&
    get_packages()
    {
        return m_packages;
    }

    std::vector<core::static_package*>&
    get_static_packages()
    {
        return m_static_packages;
    }

protected:
    std::unordered_map<utils::id, package*> m_packages;
    std::vector<std::unique_ptr<package>> m_dynamic_packages;
    std::vector<core::static_package*> m_static_packages;
};

}  // namespace core

}  // namespace agea