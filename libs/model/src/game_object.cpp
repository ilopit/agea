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

void
game_object::build_components_structure()
{
    for (auto i = 0; i < m_components.size(); ++i)
    {
        auto c = m_components[i];
        c->set_owner(this);

        if (auto gc = c->as<game_object_component>())
        {
            m_render_components.push_back(gc);
        }

        if (c->get_parent_idx() != NO_parent)
        {
            c->set_parent(m_components[c->get_parent_idx()]);
            m_components[c->get_parent_idx()]->attach(c);
        }

        set_root_component(m_components[0]->as<game_object_component>());
    }

    set_root_component(m_components[0]->as<game_object_component>());
}

bool
game_object::post_construct()
{
    AGEA_check(get_state() == smart_object_state::loaded, "Should be in proper place");

    AGEA_return_nok(base_class::post_construct());

    build_components_structure();

    m_root_component->update_matrix();

    return true;
}

}  // namespace model
}  // namespace agea
