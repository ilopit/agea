#pragma once

#include "model/smart_object.h"

#include "agea_minimal.h"

namespace agea
{
namespace model
{
class level_object;

const inline uint32_t NO_parent = UINT32_MAX;
const inline uint32_t NO_index = UINT32_MAX;

class component : public smart_object
{
public:
    AGEA_gen_class_meta(component, smart_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    bool
    construct(construct_params& c)
    {
        base_class::construct(c);

        return true;
    }

    bool clone(this_class& src);

    void
    set_owner(level_object* o)
    {
        AGEA_cond(!m_owner_obj, "Re-assign owner!");
        m_owner_obj = o;
    }

    level_object*
    owner() const
    {
        return m_owner_obj;
    }

    virtual void
    set_parent(component* c)
    {
        AGEA_cond(!m_parent, "Re-assign parent!");
        m_parent = c;
    }

    component*
    parent() const
    {
        return m_parent;
    }

    virtual void
    attach(component* c)
    {
        m_attached_components.push_back(c);
    }

    bool deserialize(json_conteiner& c);

    bool deserialize_finalize(json_conteiner& c);

    level_object* m_owner_obj = nullptr;
    component* m_parent = nullptr;

    uint32_t m_order_idx = NO_index;
    uint32_t m_parent_idx = NO_parent;

    std::vector<component*> m_attached_components;
};

}  // namespace model
}  // namespace agea
