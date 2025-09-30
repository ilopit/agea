#pragma once

#include "packages/root/component.generated.h"
#include "packages/root/smart_object.h"

#include <vector>

namespace agea
{
namespace root
{
template <typename node_t>
struct Node_Iter
{
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;

    Node_Iter(std::vector<node_t*>::iterator ptr)
        : m_itr(ptr)
    {
    }

    node_t&
    operator*() const
    {
        return **m_itr;
    }

    node_t*
    operator->()
    {
        return *m_itr;
    }

    Node_Iter&
    operator++()
    {
        m_itr++;
        return *this;
    }

    Node_Iter
    operator++(int)
    {
        Node_Iter tmp = *this;
        ++(*this);
        return tmp;
    }

    friend bool
    operator==(const Node_Iter& a, const Node_Iter& b)
    {
        return a.m_itr == b.m_itr;
    }

    friend bool
    operator!=(const Node_Iter& a, const Node_Iter& b)
    {
        return a.m_itr != b.m_itr;
    }

private:
    std::vector<node_t*>::iterator m_itr;
};

template <typename node_t>
struct Range
{
    Node_Iter<node_t>
    begin()
    {
        return m_begin;
    }

    Node_Iter<node_t>
    end()
    {
        return m_end;
    }

    Node_Iter<node_t> m_begin;
    Node_Iter<node_t> m_end;
};

class game_object;

const inline uint32_t NO_parent = UINT32_MAX;
const inline uint32_t NO_index = UINT32_MAX;

AGEA_ar_class("architype=component");
class component : public smart_object
{
    AGEA_gen_meta__component();

public:
    AGEA_gen_class_meta(component, smart_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

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
        m_owner_obj = o;
    }

    game_object*
    get_owner()
    {
        return m_owner_obj;
    }

    virtual void
    set_parent(component* c)
    {
        AGEA_check(!m_parent, "Re-assign parent!");
        m_parent = c;
    }

    virtual void
    on_tick(float)
    {
    }

    component&
    add_child(component* c)
    {
        m_children.push_back(c);

        c->set_parent(this);
        c->set_owner(m_owner_obj);

        return *this;
    }

    component*
    get_parent() const
    {
        return m_parent;
    }

    const std::vector<component*>&
    get_children() const
    {
        return m_children;
    }

    uint32_t
    get_order_idx() const
    {
        return m_order_idx;
    }

    uint32_t
    get_parent_idx() const
    {
        return m_parent_idx;
    }

    void
    set_order_parent_idx(uint32_t o, uint32_t p)
    {
        m_order_idx = o;
        m_parent_idx = p;
    }

    uint32_t
    count_child_nodes()
    {
        m_total_subcomponents = (uint32_t)m_children.size();
        for (auto n : m_children)
        {
            m_total_subcomponents += n->count_child_nodes();
        }

        return m_total_subcomponents;
    }

    game_object* m_owner_obj = nullptr;
    component* m_parent = nullptr;

    uint32_t m_order_idx = NO_index;
    uint32_t m_parent_idx = NO_parent;
    uint32_t m_total_subcomponents = 0;

    std::vector<component*> m_children;
};

}  // namespace root
}  // namespace agea
