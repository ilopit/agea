#include "model/package.h"

#include "model/object_load_context.h"

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

package::package()
    : m_occ(std::make_unique<object_load_context>())
    , m_mapping(std::make_shared<model::object_mapping>())
{
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