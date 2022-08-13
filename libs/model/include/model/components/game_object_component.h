#pragma once

#include "game_object_component.generated.h"

#include "model/components/component.h"
#include "model/rendering/renderable.h"
#include "model/core_types/vec3.h"

namespace agea
{
namespace model
{

const extern vec3 DEF_FORWARD;
const extern vec3 DEF_UP;
const extern vec3 DEF_RIGHT;

AGEA_class();
class game_object_component : public component, public renderable
{
    AGEA_gen_meta__game_object_component();

public:
    AGEA_gen_class_meta(game_object_component, component);
    AGEA_gen_construct_params
    {
        vec3 position = vec3(0.f, 0.f, 0.f);
        vec3 scale = vec3(1.f, 1.f, 1.f);
        vec3 rotation = vec3(0.f, 0.f, 0.f);
    };
    AGEA_gen_meta_api;

    bool
    construct(construct_params& c)
    {
        AGEA_return_nok(base_class::construct(c));

        m_position = c.position;
        m_rotation = c.rotation;
        m_scale = c.scale;

        return true;
    }

    vec3
    get_forward_vector() const
    {
        return glm::rotate(glm::quat(m_rotation.as_glm()), DEF_FORWARD.as_glm());
    }

    vec3
    get_up_vector() const
    {
        return glm::rotate(glm::quat(m_rotation.as_glm()), DEF_UP.as_glm());
    }

    vec3
    get_right_vector() const
    {
        return glm::rotate(glm::quat(m_rotation.as_glm()), DEF_RIGHT.as_glm());
    }

    void
    move(const vec3& delta)
    {
        m_position += delta.as_glm();
    }

    void
    rotate(float delta_angle, const vec3& axis)
    {
        m_rotation = glm::rotate(m_rotation, delta_angle, axis);
    }

    void
    roll(float delta_angle)
    {
        rotate(delta_angle, DEF_FORWARD);
    }

    void
    yaw(float delta_angle)
    {
        rotate(delta_angle, DEF_UP);
    }

    void
    pitch(float delta_angle)
    {
        rotate(delta_angle, DEF_RIGHT);
    }

    void
    register_for_rendering();

    virtual void
    set_parent(component* c)
    {
        base_class::set_parent(c);

        if (c->castable_to<game_object_component>())
        {
            m_render_root = (game_object_component*)c;
        }
    }

    virtual void
    attach(component* c)
    {
        base_class::attach(c);

        m_render_components.push_back((game_object_component*)c);
    }

    virtual bool
    prepare_for_rendering() override;

    void
    update_matrix();

    glm::mat4
    get_transofrm_matrix();

    virtual void
    editor_update() override;

    // ptr access for binding
    vec3*
    position_ptr()
    {
        return &m_position;
    }

    vec3*
    rotation_ptr()
    {
        return &m_rotation;
    }
    vec3*
    scale_ptr()
    {
        return &m_scale;
    }

    bool
    is_renderable()
    {
        return *m_renderable;
    }

protected:
    AGEA_property("category=world", "serializable=true", "hint=x,y,z");
    vec3 m_position;

    AGEA_property("category=world", "serializable=true", "hint=x,y,z");
    vec3 m_rotation;

    AGEA_property("category=world", "serializable=true", "hint=x,y,z");
    vec3 m_scale;

    AGEA_property("category=render", "ref=true");
    bool* m_visible = nullptr;

    AGEA_property("category=render", "serializable=true", "ref=true");
    bool* m_renderable = nullptr;

    // clang-format off
    std::vector<game_object_component*>      m_render_components;

    game_object_component*                   m_render_root = nullptr;
    // clang-format on
};

}  // namespace model
}  // namespace agea
