#pragma once

#include "core/agea_minimal.h"

#include "utils/weird_singletone.h"
#include "model/model_fwds.h"
#include "model/smart_object.h"

#include <string>

namespace agea
{
namespace model
{

using smart_object_ptr = std::shared_ptr<smart_object>;

class line_cache
{
public:
    smart_object*
    get_item(const core::id& id)
    {
        auto itr = std::find_if(m_items.begin(), m_items.end(),
                                [&id](const smart_object_ptr& o) { return o->get_id() == id; });

        return itr != m_items.end() ? itr->get() : nullptr;
    }

    bool
    has_item(const core::id& id)
    {
        return get_item(id) == nullptr;
    }

    void
    add_item(smart_object_ptr obj)
    {
        m_items.push_back(std::move(obj));
    }

    uint32_t
    get_size()
    {
        return (uint32_t)m_items.size();
    }

    const std::vector<smart_object_ptr>&
    get_items() const
    {
        return m_items;
    }

protected:
    std::vector<smart_object_ptr> m_items;
    bool m_strict_mode;
};
}  // namespace model
}  // namespace agea