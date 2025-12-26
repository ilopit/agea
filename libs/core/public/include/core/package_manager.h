#pragma once

#include "core/model_minimal.h"
#include "core/model_fwds.h"

#include <utils/singleton_instance.h>

#include <unordered_set>

namespace agea
{
namespace core
{

class package;
using static_package_loader = std::unique_ptr<package> (*)();

struct static_package_context
{
    static_package_context();
    ~static_package_context();

    static_package_loader loader;
    std::unique_ptr<package> pkg;
};

class package_manager
{
public:
    package_manager();
    ~package_manager();

    bool
    init();

    bool
    deinit();

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
    register_package(package& pkg);

    template <typename T>
    void
    register_static_package_loader()
    {
        auto& lctx = m_static_packages[T::package_id()];
        lctx.loader = T::package_loader();
    }

    template <typename T>
    package&
    load_static_package()
    {
        return load_static_package(T::package_id());
    }

    package&
    load_static_package(const utils::id& package_id);

    std::unordered_map<utils::id, package*>&
    get_packages()
    {
        return m_packages;
    }

protected:
    std::unordered_map<utils::id, package*> m_packages;
    std::vector<std::unique_ptr<package>> m_dynamic_packages;
    std::unordered_map<utils::id, static_package_context> m_static_packages;
    std::vector<package*> m_sorted_static_packages;
};

}  // namespace core

}  // namespace agea