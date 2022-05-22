#include "model/game_object.h"

#include "model/components/component.h"
#include "model/components/game_object_component.h"

#include <json/json.h>

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
game_object::update_matrixes()
{
    m_root_component->update_matrix();
}

void
game_object::update()
{
    update_matrixes();
}

void
game_object::prepare_for_rendering()
{
    m_root_component->prepare_for_rendering();
}

void
game_object::build_components_structure()
{
    static int ii = 0;
    for (auto c : m_components)
    {
        if (c->id().empty())
        {
            c->m_id = m_id + "/" + std::to_string(ii);
            ++ii;
        }
    }

    for (auto i = 0; i < m_components.size(); ++i)
    {
        auto cloned = m_components[i];
        cloned->set_owner(this);

        if (cloned->m_parent_idx != NO_parent)
        {
            cloned->set_parent(m_components[cloned->m_parent_idx]);
            m_components[cloned->m_parent_idx]->attach(cloned);
        }
    }

    set_root_component(m_components[0]->as<game_object_component>());

    m_position = &m_root_component->m_position;
    m_rotation = &m_root_component->m_rotation;
    m_scale = &m_root_component->m_scale;
}

bool
game_object::post_construct()
{
    AGEA_return_nok(base_class::post_construct());

    for (auto c : m_components)
    {
        c->META_post_construct();
    }

    build_components_structure();

    return true;
}

}  // namespace model
}  // namespace agea
