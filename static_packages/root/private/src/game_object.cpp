#include "packages/root/game_object.h"

#include "packages/root/components/component.h"
#include "packages/root/components/game_object_component.h"

#include <core/object_constructor.h>
#include <core/package.h>
#include <core/level.h>
#include <core/object_load_context.h>

namespace agea
{
namespace root
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
    if (!base_class::construct(params))
    {
        return false;
    }

    game_object_component::construct_params gcp;
    gcp.position = params.pos;

    m_root_component = spawn_component<game_object_component>(nullptr, AID("root_component"), gcp);

    return true;
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

component*
game_object::spawn_component(component* parent,
                             const utils::id& type_id,
                             const utils::id& id,
                             const component::construct_params& params)
{
    AGEA_check(((bool)m_package != (bool)m_level), "Only one should be set!");

    auto comp = core::object_constructor::object_construct(
                    type_id, id, params,
                    m_package ? m_package->get_load_context() : m_level->get_load_context())
                    ->as<component>();

    if (parent)
    {
        parent->add_child(comp);
    }

    return comp;
}

component*
game_object::spawn_component_with_proto(component* parent,
                                        const utils::id& proto_id,
                                        const utils::id& id)
{
    auto& occ = m_package ? m_package->get_load_context() : m_level->get_load_context();

    auto proto_obj = occ.find_obj(proto_id);

    if (!proto_obj)
    {
        return nullptr;
    }

    root::smart_object* result = nullptr;

    auto rc = core::object_constructor::object_clone_create_internal(*proto_obj, id, occ, result);
    if (rc != result_code::ok)
    {
        return nullptr;
    }

    occ.reset_loaded_objects();

    if (!result->post_load())
    {
        return nullptr;
    }

    auto com = result->as<component>();

    if (parent)
    {
        parent->add_child(com);
    }

    return com;
}

component*
game_object::spawn_component_with_proto(component* parent, const utils::id& proto_id)
{
    return spawn_component_with_proto(parent, proto_id,
                                      glob::id_generator::getr().generate(m_id, proto_id));
}

bool
game_object::post_construct()
{
    AGEA_check(get_state() != smart_object_state::constructed, "Should be in proper place");

    recreate_structure_from_layout();

    set_state(smart_object_state::constructed);

    update_position();

    return true;
}

bool
game_object::post_load()
{
    AGEA_check(get_state() == smart_object_state::loaded, "Should be in proper place");

    recreate_structure_from_ids();

    for (auto c : m_components)
    {
        c->set_state(smart_object_state::constructed);
    }

    set_state(smart_object_state::constructed);

    update_position();

    return true;
}

void
game_object::update_position()
{
    for (auto c : m_renderable_components)
    {
        c->update_matrix();
    }
}

void
game_object::recreate_structure_form_layout_impl(component* parent,
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
        recreate_structure_form_layout_impl(child, position, subojects_count);
        parent->m_total_subcomponents += subojects_count;
    }

    parent->m_total_subcomponents += (uint32_t)parent->m_children.size();
    total_subojects_count = parent->m_total_subcomponents;
}

void
game_object::set_rotation(vec3 v)
{
    m_root_component->set_rotation(v);
}

void
game_object::recreate_structure_from_layout()
{
    uint32_t last_obj_id = 0, subobj_count = 0;

    m_root_component->m_owner_obj = this;

    m_components.clear();

    recreate_structure_form_layout_impl(m_root_component, last_obj_id, subobj_count);
    fill_renderable_components();
}

void
game_object::fill_renderable_components()
{
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
            m_root_component->set_owner(this);
        }
    }

    m_root_component->count_child_nodes();

    fill_renderable_components();
}

void
game_object::update_root()
{
    m_root_component = m_components.front()->as<game_object_component>();
    m_root_component->set_owner(this);
}

void
game_object::move(const vec3& v)
{
    m_root_component->move(v);
}

}  // namespace root
}  // namespace agea
