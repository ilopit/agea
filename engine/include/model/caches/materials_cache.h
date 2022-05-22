#pragma once

#include "core/agea_minimal.h"

#include "model/rendering/material.h"

namespace agea
{
namespace model
{
class materials_cache
{
public:
    void
    init();

    std::shared_ptr<model::material>
    get(const std::string& id);

    std::unordered_map<std::string, std::shared_ptr<model::material>> m_materials;
};

}  // namespace model

namespace glob
{
struct materials_cache : public weird_singleton<::agea::model::materials_cache>
{
};
}  // namespace glob

}  // namespace agea