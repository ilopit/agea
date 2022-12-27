#include "model/package_manager.h"

namespace agea
{

glob::package_manager::type glob::package_manager::type::s_instance;

namespace model
{

bool
package_manager::load_package(const utils::id& id)
{
    auto itr = m_packages.find(id);
    if (itr != m_packages.end())
    {
        return true;
    }

    auto path = glob::resource_locator::get()->resource(category::packages, id.str());

    auto p = std::make_unique<model::package>();
    if (!package::load_package(path, *p, glob::class_objects_cache_set_view::getr(),
                               glob::objects_cache_set_view::getr()))
    {
        return false;
    }

    p->propagate_to_global_caches();

    m_packages[id] = std::move(p);

    return true;
}

package*
package_manager::get_package(const utils::id& id)
{
    auto itr = m_packages.find(id);

    return itr != m_packages.end() ? (itr->second.get()) : nullptr;
}

}  // namespace model
}  // namespace agea