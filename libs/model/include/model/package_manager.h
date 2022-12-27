#pragma once

#include "model/model_minimal.h"

#include "model/model_fwds.h"
#include "model/package.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace model
{
class package_manager
{
public:
    bool
    load_package(const utils::id& id);

    package*
    get_package(const utils::id& id);

protected:
    std::unordered_map<utils::id, std::unique_ptr<package>> m_packages;
};

}  // namespace model

namespace glob
{
struct package_manager
    : public ::agea::singleton_instance<::agea::model::package_manager, package_manager>
{
};
}  // namespace glob
}  // namespace agea