#include "model/package_manager.h"

namespace agea
{
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
    if (!package::load_package(path, *p, glob::class_objects_cache_set_view::get(),
                               glob::objects_cache_set_view::get()))
    {
        return false;
    }

    if (!p->prepare_for_rendering())
    {
        return false;
    }

    p->propagate_to_global_caches();

    m_packages[id] = std::move(p);

    return true;
}

}  // namespace model
}  // namespace agea