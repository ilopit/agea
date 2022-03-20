#include "level_object.h"

#include "components/component.h"
#include "model/components/level_object_component.h"
#include "model/components/empty_level_object_component.h"

#include <json/json.h>

namespace agea
{
namespace model
{
bool
level_object::clone(this_class& src)
{
    AGEA_return_nok(base_class::clone(src));

    m_components.resize(src.m_components.size());

    for (auto i = 0; i < src.m_components.size(); ++i)
    {
        auto cloned = cast_ref<component>(src.m_components[i]->META_clone_obj());

        m_components[cloned->m_order_idx] = cloned;
    }

    set_root_component(m_components[0]->as<level_object_component>());

    return true;
}

void
level_object::create_default_root()
{
    m_components.resize(1);

    auto obj = empty_level_object_component::META_class_create_empty_obj();
    obj->META_set_id("empty_root");

    auto ptr = obj->META_class_reflection_table();

    m_components[0] = obj;

    obj->m_order_idx = 0;
    obj->m_parent_idx = NO_parent;

    empty_level_object_component::construct_params c;
    m_components.front()->META_construct(c);
    set_root_component(obj.get());
}

void
level_object::update_matrixes()
{
    m_root_component->update_matrix();
}

void
level_object::update()
{
    update_matrixes();
}

void
level_object::prepare_for_rendering()
{
    m_root_component->prepare_for_rendering();
}

void
level_object::build_components_structure()
{
    for (auto i = 0; i < m_components.size(); ++i)
    {
        auto cloned = m_components[i];
        cloned->set_owner(this);

        if (cloned->m_parent_idx != NO_parent)
        {
            cloned->set_parent(m_components[cloned->m_parent_idx].get());
            m_components[cloned->m_parent_idx]->attach(cloned.get());
        }
    }
}

bool
level_object::post_construct()
{
    AGEA_return_nok(base_class::post_construct());

    build_components_structure();

    return true;
}

bool
level_object::deserialize(json_conteiner& c)
{
    AGEA_return_nok(base_class::deserialize(c));

    create_default_root();

    return true;
}

bool
level_object::deserialize_finalize(json_conteiner& c)
{
    AGEA_return_nok(base_class::deserialize_finalize(c));

    m_root_component->deserialize_finalize(c);

    return true;
}

}  // namespace model
}  // namespace agea
