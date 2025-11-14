#pragma once

#include "packages/root/model/smart_object.h"

#include "core/architype.h"

#include <utils/singleton_instance.h>

#include <string>

namespace agea
{
namespace core
{

template <typename id_t>
class hash_cache
{
public:
    root::smart_object*
    get_item(const id_t& id)
    {
        auto item = m_items.find(id);

        return item != m_items.end() ? item->second : nullptr;
    }

    void
    add_item(const root::smart_object& obj)
    {
        auto& item = m_items[obj.get_id()];

        AGEA_check(item == nullptr, "Should not re-assign!");
        item = (root::smart_object*)&obj;
    }

    void
    remove_item(const root::smart_object& obj)
    {
        const auto& oid = obj.get_id();
        m_items.erase(oid);
    }
    bool
    has_item(const id_t& id)
    {
        return m_items.find(id) != m_items.end();
    }

    uint32_t
    get_size() const
    {
        return (uint32_t)m_items.size();
    }

    const std::unordered_map<id_t, root::smart_object*>&
    get_items() const
    {
        return m_items;
    }

    void
    clear()
    {
        m_items.clear();
    }

protected:
    std::unordered_map<id_t, root::smart_object*> m_items;
};

class architype_cache : public hash_cache<utils::id>
{
public:
    architype_cache(architype id)
        : m_id(id)
    {
    }

    void
    add_item(const root::smart_object& obj)
    {
        AGEA_check(obj.get_architype_id() == get_id() || get_id() == architype::smart_object,
                   "Should have same architype!");

        hash_cache::add_item(obj);
    }

    void
    remove_item(const root::smart_object& obj)
    {
        AGEA_check(obj.get_architype_id() == get_id() || get_id() == architype::smart_object,
                   "Should have same architype!");

        hash_cache::remove_item(obj);
    }

    architype
    get_id() const
    {
        return m_id;
    }

protected:
    architype m_id;
};
}  // namespace core
}  // namespace agea