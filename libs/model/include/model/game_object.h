#pragma once

#include "model/game_object.generated.h"

#include "model/model_minimal.h"
#include "model/core_types/vec3.h"
#include "model/components/game_object_component.h"
#include "model/smart_object.h"

namespace agea
{
namespace model
{
class component;

AGEA_class();
class game_object : public smart_object
{
    AGEA_gen_meta__game_object();

public:
    // Meta part
    AGEA_gen_class_meta(game_object, smart_object);
    AGEA_gen_construct_params{

    };
    AGEA_gen_meta_architype_api(game_object);

    bool
    construct(construct_params& params);

    virtual bool
    post_construct() override;

    virtual bool
    post_load() override;

    void
    update_position();

    void
    attach(component* c);

    component*
    spawn_component(const utils::id& type_id,
                    const utils::id& id,
                    const model::component::construct_params& params);

    virtual void
    on_tick(float dt)
    {
    }

    component*
    get_component_at(size_t idx) const
    {
        AGEA_check(idx < m_components.size(), "Index should be in range");
        return m_components[idx];
    }

    AGEA_function("category=world");
    glm::quat
    get_rotation_quat() const
    {
        return m_root_component->get_rotation();
    }

    AGEA_function("category=world");
    vec3
    get_forward_vector() const
    {
        return m_root_component->get_forward_vector();
    }

    AGEA_function("category=world");
    vec3
    get_up_vector() const
    {
        return m_root_component->get_up_vector();
    }

    AGEA_function("category=world");
    vec3
    get_right_vector() const
    {
        return m_root_component->get_right_vector();
    }

    void
    set_root_component(game_object_component* root)
    {
        m_root_component = root;
    }

    game_object_component*
    get_root_component()
    {
        return m_root_component;
    }

    std::vector<game_object_component*>&
    get_renderable_components()
    {
        return m_renderable_components;
    }

    void
    recreate_structure_from_layout();

    void
    recreate_structure_from_ids();

    Range<component>
    get_components()
    {
        return {m_components.begin(), m_components.end()};
    }

protected:
    void
    recreate_structure_form_layout_impl(component* parent,
                                        uint32_t& position,
                                        uint32_t& total_subojects_count);

    void
    fill_renderable_components();

    AGEA_property("category=meta",
                  "serializable=true",
                  "property_des_handler=custom::game_object_components_deserialize",
                  "property_ser_handler=custom::game_object_components_serialize",
                  "property_prototype_handler=custom::game_object_components_prototype",
                  "property_compare_handler=custom::game_object_components_compare",
                  "property_copy_handler=custom::game_object_components_copy");
    std::vector<component*> m_components;

    std::vector<game_object_component*> m_renderable_components;

    game_object_component* m_root_component = nullptr;

    bool m_updated = false;
};

}  // namespace model
}  // namespace agea
