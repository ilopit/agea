#pragma once

#include "component.generated.h"

#include "model/smart_object.h"

namespace agea
{
namespace model
{
class game_object;

const inline uint32_t NO_parent = UINT32_MAX;
const inline uint32_t NO_index = UINT32_MAX;

class component : public smart_object
{
    AGEA_gen_meta__component();

public:
    AGEA_gen_class_meta(component, smart_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_architype_api(component);

    bool
    construct(construct_params& c)
    {
        if (!base_class::construct(c))
        {
            return false;
        }

        return true;
    }

    void
    set_owner(game_object* o)
    {
        AGEA_check(!m_owner_obj, "Re-assign owner!");
        m_owner_obj = o;
    }

    game_object*
    get_owner() const
    {
        return m_owner_obj;
    }

    virtual void
    set_parent(component* c)
    {
        AGEA_check(!m_parent, "Re-assign parent!");
        m_parent = c;
    }

    component*
    get_parent() const
    {
        return m_parent;
    }

    virtual void
    attach(component* c)
    {
        m_attached_components.push_back(c);
    }

    const std::vector<component*>
    get_attached_components() const
    {
        return m_attached_components;
    }

protected:
    game_object* m_owner_obj = nullptr;
    component* m_parent = nullptr;

    AGEA_property("category=meta", "serializable=true");
    uint32_t m_order_idx = NO_index;

    AGEA_property("category=meta", "serializable=true");
    uint32_t m_parent_idx = NO_parent;

    std::vector<component*> m_attached_components;
};

}  // namespace model
}  // namespace agea
