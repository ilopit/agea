#pragma once

#include "model/model_fwds.h"
#include "model/smart_object.h"
#include "model/architype.h"

#include <utils/singleton_instance.h>

#include <string>

namespace agea
{
namespace model
{
class hash_cache
{
public:
    hash_cache(architype id)
        : m_id(id)
    {
    }

    smart_object*
    get_item(const utils::id& id)
    {
        auto item = m_items.find(id);

        return item != m_items.end() ? item->second : nullptr;
    }

    bool
    has_item(const utils::id& id)
    {
        return m_items.find(id) != m_items.end();
    }

    void
    add_item(smart_object& obj)
    {
        AGEA_check(obj.get_architype_id() == get_id() || get_id() == architype::smart_object,
                   "Should have same architype!");

        auto& item = m_items[obj.get_id()];

        AGEA_check(item == nullptr, "Should not re-assign!");
        item = &obj;
    }

    architype
    get_id() const
    {
        return m_id;
    }

    uint32_t
    get_size()
    {
        return (uint32_t)m_items.size();
    }

    const std::unordered_map<utils::id, smart_object*>&
    get_items()
    {
        return m_items;
    }

protected:
    std::unordered_map<utils::id, smart_object*> m_items;
    architype m_id;
};
}  // namespace model
}  // namespace agea