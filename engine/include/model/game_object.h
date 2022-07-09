#pragma once

#include "core/agea_minimal.h"

#include "model/components/game_object_component.h"

#include <iostream>
#include <vector>

namespace agea
{
namespace model
{
class component;

class game_object : public smart_object
{
public:
    // Meta part
    AGEA_gen_class_meta(game_object, smart_object);
    AGEA_gen_construct_params{

    };
    AGEA_gen_meta_architype_api(game_object);

    bool
    construct(construct_params& params)
    {
        return base_class::construct(params);
    }

    // Position
    glm::vec3
    get_position() const
    {
        return m_root_component->get_position();
    }

    void
    set_position(const glm::vec3& p)
    {
        m_root_component->set_position(p);
    }

    glm::quat
    get_rotation() const
    {
        return m_root_component->get_rotation();
    }

    void
    set_rotation(glm::quat q)
    {
        m_root_component->set_rotation(q);
    }

    glm::vec3
    get_forward_vector() const
    {
        return m_root_component->get_forward_vector();
    }

    glm::vec3
    get_up_vector() const
    {
        return m_root_component->get_up_vector();
    }

    glm::vec3
    get_right_vector() const
    {
        return m_root_component->get_right_vector();
    }

    void
    move(const glm::vec3& delta)
    {
        m_root_component->move(delta);
    }

    void
    rotate(float delta_angle, const glm::vec3& axis)
    {
        m_root_component->rotate(delta_angle, axis);
    }

    void
    roll(float delta_angle)
    {
        m_root_component->roll(delta_angle);
    }

    void
    yaw(float delta_angle)
    {
        m_root_component->yaw(delta_angle);
    }

    void
    pitch(float delta_angle)
    {
        m_root_component->pitch(delta_angle);
    }

    virtual void
    on_tick(float)
    {
    }

    // State
    void
    reset_updated_state()
    {
        m_updated = false;
    }

    bool
    has_updated() const
    {
        return m_updated;
    }

    void
    mark_updated()
    {
        m_updated = true;
    }

    bool
    consume_update()
    {
        auto prev = has_updated();
        reset_updated_state();

        return prev;
    }

    void
    attach_component(component* c)
    {
        c->set_owner(this);
    }

    template <typename obj_type>
    component*
    find_component()
    {
        for (auto& c : m_components)
        {
            if (c->class_id() == obj_type::META_class_id())
            {
                return c;
            }
        }

        return nullptr;
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

    void
    update_matrixes();

    void
    update();

    void
    prepare_for_rendering();

    void
    build_components_structure();

    bool
    post_construct();

    component*
    get_component_at(size_t idx) const
    {
        AGEA_check(idx < m_components.size(), "Index should be in range");
        return m_components[idx];
    }

    std::vector<component*>
    get_components()
    {
        return m_components;
    }

protected:
    AGEA_property("category=world", "visible=true", "access=rw", "hint=x,y,z");
    glm::vec3* m_position = nullptr;

    AGEA_property("category=world", "visible=true", "access=rw", "hint=x,y,z");
    glm::vec3* m_rotation = nullptr;

    AGEA_property("category=world", "visible=true", "access=rw", "hint=x,y,z");
    glm::vec3* m_scale = nullptr;

    AGEA_property("category=meta",
                  "serializable=true",
                  "property_des_handler=custom::game_object_components_deserialize",
                  "property_ser_handler=custom::game_object_components_serialize",
                  "property_prototype_handler=custom::game_object_components_prototype",
                  "property_compare_handler=custom::game_object_components_compare",
                  "property_copy_handler=custom::game_object_components_copy");
    std::vector<component*> m_components;

    game_object_component* m_root_component = nullptr;

    bool m_updated = false;
};

}  // namespace model
}  // namespace agea
