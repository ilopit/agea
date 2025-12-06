#pragma once

#include "core/model_minimal.h"
#include "core/model_fwds.h"

#include "core/architype.h"
#include "core/caches/hash_cache.h"

#include <utils/singleton_instance.h>
#include <utils/agea_log.h>

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

struct objects_cache : public cache<root::smart_object, architype::smart_object>
{
};

struct components_cache : public cache<root::component, architype::component>
{
};

struct game_objects_cache : public cache<root::game_object, architype::game_object>
{
};
struct textures_cache : public cache<root::texture, architype::texture>
{
};
struct meshes_cache : public cache<root::mesh, architype::mesh>
{
};
struct materials_cache : public cache<root::material, architype::material>
{
};
struct shader_effects_cache : public cache<root::shader_effect, architype::shader_effect>
{
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

    void
    remove_item(const root::smart_object& obj)
    {
        ALOG_INFO("Removing {0}", obj.get_id().cstr());

        auto aid = obj.get_architype_id();

        if (aid != architype::smart_object)
        {
            auto& item = m_mapping.at(aid);
            item->remove_item(obj);
        }

        m_mapping.at(architype::smart_object)->remove_item(obj);
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
}  // namespace agea