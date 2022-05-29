#pragma once

#include "core/agea_minimal.h"
#include "utils/weird_singletone.h"

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

    template <typename T>
    void
    call_on_items(const T& t)
    {
        for (auto& i : m_items)
        {
            t(i.second.get());
        }
    }

protected:
    std::unordered_map<std::string, std::shared_ptr<model::material>> m_items;
};

}  // namespace model

namespace glob
{
struct materials_cache : public weird_singleton<::agea::model::materials_cache>
{
};
}  // namespace glob

}  // namespace agea