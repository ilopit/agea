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
class base_cache
{
public:
    base_cache(architype id)
        : m_id(id)
    {
    }

    smart_object*
    get_item(const std::string& id)
    {
        auto item = m_items.find(id);

        return item != m_items.end() ? item->second : nullptr;
    }

    bool
    has_item(const std::string& id)
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

    const std::unordered_map<std::string, smart_object*>&
    get_items()
    {
        return m_items;
    }

protected:
    std::unordered_map<std::string, smart_object*> m_items;
    architype m_id;
};

template <typename T, architype ID>
class cache : public base_cache
{
public:
    cache()
        : base_cache(ID)
    {
    }

    T*
    get_item(const std::string& id)
    {
        return (T*)base_cache::get_item(id);
    }

    template <typename F>
    bool
    call_on_items(F f)
    {
        for (auto& i : m_items)
        {
            if (!f((T*)i.second))
            {
                return false;
            }
        }
        return true;
    }

    const std::unordered_map<std::string, smart_object*>&
    get_items()
    {
        return m_items;
    }
};

class caches_map
{
public:
    base_cache&
    get_cache(architype id)
    {
        return *m_mapping.at(id);
    }

    void
    add_cache(base_cache* c)
    {
        auto& v = m_mapping[c->get_id()];
        AGEA_check(v == nullptr, "Should not re-assign!");
        v = c;
    }

    void
    add_item(smart_object& obj)
    {
        auto& item = m_mapping.at(obj.get_architype_id());
        item->add_item(obj);
    }

    std::unordered_map<architype, base_cache*>&
    get_items()
    {
        return m_mapping;
    }

private:
    std::unordered_map<architype, base_cache*> m_mapping;
};

}  // namespace model

namespace glob
{
struct caches_map : public simple_singleton<::agea::model::caches_map*>
{
};
}  // namespace glob
}  // namespace agea