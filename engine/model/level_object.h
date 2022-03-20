#pragma once

#include "agea_minimal.h"

#include "model/components/level_object_component.h"
#include "model/level.h"

#include <iostream>
#include <vector>

namespace agea
{
namespace model
{
class component;

class level_object : public smart_object
{
public:
    // Meta part
    AGEA_gen_class_meta(level_object, smart_object);
    AGEA_gen_construct_params{

    };
    AGEA_gen_meta_api;

    bool
    construct(construct_params& params)
    {
        return base_class::construct(params);
    }

    bool clone(this_class& src);

    // Position
    glm::vec3
    position() const
    {
        return m_root_component->position();
    }

    void
    set_position(const glm::vec3& p)
    {
        m_root_component->set_position(p);
    }

    glm::quat
    rotation() const
    {
        return m_root_component->rotation();
    }

    void
    set_rotation(glm::quat q)
    {
        m_root_component->set_rotation(q);
    }

    glm::vec3
    forward_vector() const
    {
        return m_root_component->forward_vector();
    }

    glm::vec3
    up_vector() const
    {
        return m_root_component->up_vector();
    }

    glm::vec3
    right_vector() const
    {
        return m_root_component->right_vector();
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
    on_tick(float f)
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

    // Components part
    template <typename component_type>
    component_type*
    add_component(typename component_type::construct_params& p)
    {
        auto cobj = glob::level::get()->spawn_component<component_type>(p);
        if (cobj)
        {
            // m_components.push_back(cobj);
            cobj->set_owner(this);
        }
        return cobj;
    }

    void
    attach_component(component* c)
    {
        // m_components.push_back(c);
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

    void create_default_root();

    void
    set_root_component(level_object_component* root)
    {
        m_root_component = root;
    }

    level_object_component*
    root_component()
    {
        return m_root_component;
    }

    void update_matrixes();

    void update();

    void prepare_for_rendering();

    void build_components_structure();

    bool post_construct();

    std::vector<std::shared_ptr<component>> m_components;

    bool deserialize(json_conteiner& c);

    bool deserialize_finalize(json_conteiner& c);

    level_object_component* m_root_component = nullptr;

    bool m_updated = false;
};

}  // namespace model
}  // namespace agea
