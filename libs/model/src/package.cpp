#include "model/package.h"

#include "model/caches/empty_objects_cache.h"
#include "model/object_load_context.h"
#include "model/object_constructor.h"

#include <utils/agea_log.h>

#include <serialization/serialization.h>

#include <map>
#include <filesystem>

namespace agea
{
namespace model
{

package::package(package&&) noexcept = default;

package&
package::operator=(package&&) noexcept = default;

package::package(const utils::id& id, cache_set* class_global_set, cache_set* instance_global_set)
    : m_occ(std::make_unique<object_load_context>())
    , m_mapping(std::make_shared<model::object_mapping>())
    , m_id(id)
{
    m_occ->set_package(this)
        .set_class_local_set(&m_class_local_set)
        .set_instance_local_set(&m_instance_local_set)
        .set_ownable_cache(&m_objects)
        .set_class_global_set(class_global_set)
        .set_instance_global_set(instance_global_set);
}

package::~package()
{
}

utils::path
package::get_relative_path(const utils::path& p) const
{
    return p.relative(m_save_root_path);
}

}  // namespace model
}  // namespace agea