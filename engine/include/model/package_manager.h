#pragma once

#include "core/agea_minimal.h"

#include "model/model_fwds.h"
#include "model/package.h"
#include "utils/weird_singletone.h"

namespace agea
{
namespace model
{
class package_manager
{
public:
    bool
    load_package(const std::string& id);

    std::unordered_map<std::string, package> m_packages;
};

}  // namespace model

namespace glob
{
struct package_manager : public weird_singleton<::agea::model::package_manager>
{
};
}  // namespace glob
}  // namespace agea