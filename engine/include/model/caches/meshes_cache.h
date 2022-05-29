#pragma once

#include "core/agea_minimal.h"

#include "model/rendering/mesh.h"
#include "utils/weird_singletone.h"

namespace agea
{
namespace model
{
class meshes_cache
{
public:
    void
    init();

    std::shared_ptr<model::mesh>
    get(const std::string& id);

    std::unordered_map<std::string, std::shared_ptr<model::mesh>> m_meshes;
};

}  // namespace model

namespace glob
{
struct meshes_cache : public weird_singleton<::agea::model::meshes_cache>
{
};
}  // namespace glob
}  // namespace agea