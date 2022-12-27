#pragma once

#include "model/model_minimal.h"
#include "model/model_fwds.h"

#include "model/architype.h"
#include "model/caches/hash_cache.h"

#include <utils/singleton_instance.h>

namespace agea
{
namespace model
{
template <typename T, architype ID>
class cache : public hash_cache
{
public:
    cache()
        : hash_cache(ID)
    {
    }

    T*
    get_item(const utils::id& id)
    {
        return (T*)hash_cache::get_item(id);
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

    const std::unordered_map<utils::id, smart_object*>&
    get_items()
    {
        return m_items;
    }
};

class caches_map
{
public:
    hash_cache*
    get_cache(architype id)
    {
        return m_mapping.at(id);
    }

    void
    add_cache(hash_cache* c)
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

        m_mapping.at(architype::smart_object)->add_item(obj);
    }

    std::unordered_map<architype, hash_cache*>&
    get_items()
    {
        return m_mapping;
    }

private:
    std::unordered_map<architype, hash_cache*> m_mapping;
};
}  // namespace model

namespace glob
{
struct object_caches_map : public singleton_instance<::agea::model::caches_map, object_caches_map>
{
};

struct class_object_caches_map
    : public singleton_instance<::agea::model::caches_map, class_object_caches_map>
{
};
}  // namespace glob
}  // namespace agea