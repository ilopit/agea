#pragma once

#include "core/agea_minimal.h"

#include "model/components/component.h"

namespace agea
{
namespace model
{
class objects_cache
{
public:
    std::shared_ptr<smart_object>
    get(const std::string& id) const;

    void
    insert(std::shared_ptr<smart_object> obj);

    size_t
    size() const
    {
        return m_items.size();
    }

    const std::unordered_map<std::string, std::shared_ptr<smart_object>>&
    items() const
    {
        return m_items;
    }

protected:
    std::unordered_map<std::string, std::shared_ptr<smart_object>> m_items;
};

}  // namespace model

}  // namespace agea