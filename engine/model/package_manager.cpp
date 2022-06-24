#include "model/package_manager.h"

#include "core/fs_locator.h"

namespace agea
{
namespace model
{

bool
package_manager::load_package(const core::id& id)
{
    auto itr = m_packages.find(id);
    if (itr != m_packages.end())
    {
        return true;
    }

    auto path = glob::resource_locator::get()->resource(category::packages, id.str());

    package p;
    if (!package::load_package(path, p, glob::cache_set_view::get()))
    {
        return false;
    }

    auto& p_ref = m_packages[id];

    p_ref = std::move(p);
    p_ref.propagate_to_global_caches();

    return true;
}

}  // namespace model
}  // namespace agea