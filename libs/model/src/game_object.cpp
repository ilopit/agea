#include "model/game_object.h"

#include "model/components/component.h"
#include "model/components/game_object_component.h"

namespace agea
{
namespace model
{

game_object::game_object()
{
}

game_object::~game_object()
{
}

bool
game_object::construct(construct_params& params)
{
    return base_class::construct(params);
}

void
game_object::attach(component* c)
{
    if (c->get_parent_idx() == -1)
    {
        m_root_component = c->as<game_object_component>();
        AGEA_check(m_root_component, "Root should be valid");
    }

    m_components.push_back(c);
    c->set_owner(this);
}

bool
game_object::post_construct()
{
    AGEA_check(get_state() == smart_object_state::loaded, "Should be in proper place");

    AGEA_return_nok(base_class::post_construct());

    for (auto c : m_components)
    {
        c->post_construct();
    }

    recreate_structure_from_ids();

    m_root_component->update_matrix();

    return true;
}

void
game_object::recreate_structure_form_layout(component* parent,
                                            uint32_t& position,
                                            uint32_t& total_subojects_count)
{
    parent->m_order_idx = position;
    parent->m_total_subcomponents = 0;

    attach(parent);

    for (auto child : parent->m_children)
    {
        child->m_parent_idx = parent->m_order_idx;
        ++position;

        uint32_t subojects_count = 0;
        recreate_structure_form_layout(child, position, subojects_count);
        parent->m_total_subcomponents += subojects_count;
    }

    parent->m_total_subcomponents += (uint32_t)parent->m_children.size();
    total_subojects_count = parent->m_total_subcomponents;
}

void
game_object::recreate_structure_form_layout()
{
    uint32_t last_obj_id = 0, subobj_count = 0;

    m_root_component->m_owner_obj = this;

    m_components.clear();
    m_renderable_components.clear();

    recreate_structure_form_layout(m_root_component, last_obj_id, subobj_count);

    for (auto c : m_components)
    {
        if (auto goc = c->as<game_object_component>())
        {
            m_renderable_components.push_back(goc);
        }
    }
}

void
game_object::recreate_structure_from_ids()
{
    if (m_components.empty())
    {
        return;
    }

    m_root_component = nullptr;

    for (auto& n : m_components)
    {
        std::swap(n, m_components[n->m_order_idx]);
    }

    for (auto n : m_components)
    {
        if (n->m_parent_idx != -1)
        {
            m_components[n->m_parent_idx]->add_child(n);
        }
        else
        {
            AGEA_check(!m_root_component, "Should not be reassigned");
            m_root_component = n->as<game_object_component>();
        }
    }

    m_root_component->count_child_nodes();

    for (auto c : m_components)
    {
        if (auto goc = c->as<game_object_component>())
        {
            m_renderable_components.push_back(goc);
        }
    }
}

}  // namespace model
}  // namespace agea
