#pragma once

#include "model/components/component.h"
#include "model/rendering/renderable.h"

#include "core/agea_minimal.h"

namespace agea
{
namespace model
{

const extern glm::vec3 DEF_FORWARD;
const extern glm::vec3 DEF_UP;
const extern glm::vec3 DEF_RIGHT;

class game_object_component : public component, public renderable
{
public:
    AGEA_gen_class_meta(game_object_component, component);
    AGEA_gen_construct_params
    {
        glm::vec3 position = glm::vec3(0.f, 0.f, 0.f);
        glm::vec3 scale = glm::vec3(1.f, 1.f, 1.f);
        glm::vec3 rotation = glm::vec3(0.f, 0.f, 0.f);
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

    glm::vec3
    get_position() const
    {
        return m_position;
    }

    void
    set_position(glm::vec3 p)
    {
        m_position = p;
    }

    glm::vec3
    get_rotation() const
    {
        return m_rotation;
    }

    void
    set_rotation(glm::quat)
    {
        // m_rotation = q;
    }

    glm::vec3
    get_scale() const
    {
        return m_scale;
    }

    void
    set_scale(glm::vec3 s)
    {
        m_scale = s;
    }

    glm::vec3
    get_forward_vector() const
    {
        return glm::rotate(glm::quat(m_rotation), DEF_FORWARD);
    }

    glm::vec3
    get_up_vector() const
    {
        return glm::rotate(glm::quat(m_rotation), DEF_UP);
    }

    glm::vec3
    get_right_vector() const
    {
        return glm::rotate(glm::quat(m_rotation), DEF_RIGHT);
    }

    void
    move(const glm::vec3& delta)
    {
        m_position += delta;
    }

    void
    rotate(float delta_angle, const glm::vec3& axis)
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
    glm::vec3*
    position_ptr()
    {
        return &m_position;
    }

    glm::vec3*
    rotation_ptr()
    {
        return &m_rotation;
    }
    glm::vec3*
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
    AGEA_property("category=world", "serializable=true", "visible=true", "access=rw", "hint=x,y,z");
    glm::vec3 m_position;

    AGEA_property("category=world", "serializable=true", "visible=true", "access=rw", "hint=x,y,z");
    glm::vec3 m_rotation;

    AGEA_property("category=world", "serializable=true", "visible=true", "access=rw", "hint=x,y,z");
    glm::vec3 m_scale;

    AGEA_property("category=render", "visible=true");
    bool* m_visible = nullptr;

    AGEA_property("category=render", "serializable=true", "visible=true");
    bool* m_renderable = nullptr;

    // clang-format off
    std::vector<game_object_component*>      m_render_components;

    game_object_component*                   m_render_root = nullptr;
    // clang-format on
};

}  // namespace model
}  // namespace agea
