#pragma once

#include "core/model_minimal.h"
#include "core/model_fwds.h"

#include "core/architype.h"
#include "core/caches/hash_cache.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace core
{
template <typename T, architype ID>
class cache : public architype_cache
{
public:
    cache()
        : architype_cache(ID)
    {
    }

    T*
    get_item(const utils::id& id)
    {
        return (T*)architype_cache::get_item(id);
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

    const std::unordered_map<utils::id, root::smart_object*>&
    get_items()
    {
        return m_items;
    }
};

class caches_map
{
public:
    architype_cache*
    get_cache(architype id)
    {
        return m_mapping.at(id);
    }

    void
    add_cache(architype_cache* c)
    {
        auto& v = m_mapping[c->get_id()];
        AGEA_check(v == nullptr, "Should not re-assign!");
        v = c;
    }

    void
    add_item(root::smart_object& obj)
    {
        auto aid = obj.get_architype_id();

        if (aid != architype::smart_object)
        {
            auto& item = m_mapping.at(aid);
            item->add_item(obj);
        }

        m_mapping.at(architype::smart_object)->add_item(obj);
    }

    std::unordered_map<architype, architype_cache*>&
    get_items()
    {
        return m_mapping;
    }

private:
    std::unordered_map<architype, architype_cache*> m_mapping;
};
}  // namespace core

namespace glob
{
struct object_caches_map : public singleton_instance<::agea::core::caches_map, object_caches_map>
{
};

struct proto_object_caches_map
    : public singleton_instance<::agea::core::caches_map, proto_object_caches_map>
{
};
}  // namespace glob
}  // namespace agea