#pragma once

#include "root/game_object_component.generated.h"

#include "root/components/component.h"
#include "root/core_types/vec3.h"

namespace agea
{

namespace render
{
class object_data;
}

namespace root
{

const extern vec3 DEF_FORWARD;
const extern vec3 DEF_UP;
const extern vec3 DEF_RIGHT;

AGEA_ar_class();
class game_object_component : public component
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
    get_forward_vector() const;
    vec3
    get_up_vector() const;
    vec3
    get_right_vector() const;

    void
    move(const vec3& delta);
    void
    rotate(const vec3& delta);

    const glm::mat4&
    get_transofrm_matrix() const
    {
        return m_transform_matrix;
    }
    const glm::mat4&
    get_normal_matrix() const
    {
        return m_normal_matrix;
    }
    const glm::vec4&
    get_world_position() const
    {
        return m_world_position;
    };

    void
    update_matrix();

    virtual void
    set_parent(component* c)
    {
        base_class::set_parent(c);

        if (c->castable_to<game_object_component>())
        {
            m_render_root = (game_object_component*)c;
        }
    }

    std::vector<component*>&
    get_render_components()
    {
        return m_components;
    }

    render::object_data*
    get_render_object_data() const
    {
        return m_render_handle;
    }
    void
    set_render_object_data(render::object_data* v)
    {
        m_render_handle = v;
    }
    bool
    has_dirty_transform() const
    {
        return m_has_dirty_transform;
    }
    void
    set_dirty_transform(bool v)
    {
        m_has_dirty_transform = v;
    }

    void
    mark_transform_dirty();

    void
    mark_render_dirty();

protected:
    AGEA_ar_property("category=object", "serializable=true", "default=true");
    bool m_tickable = false;

    AGEA_ar_property("category=object", "serializable=true", "default=true");
    bool m_visible = false;

    AGEA_ar_property("category=world", "serializable=true", "hint=x,y,z", "default=true");
    vec3 m_position = {0.f};

    AGEA_ar_property("category=world", "serializable=true", "hint=x,y,z", "default=true");
    vec3 m_rotation = {0.f};

    AGEA_ar_property("category=world", "serializable=true", "hint=x,y,z", "default=true");
    vec3 m_scale = {1.f};

    glm::mat4 m_transform_matrix;
    glm::mat4 m_normal_matrix;
    glm::vec4 m_world_position;

    std::vector<component*> m_components;

    bool m_has_dirty_transform = false;

    game_object_component* m_render_root = nullptr;
    render::object_data* m_render_handle = nullptr;
};

}  // namespace root
}  // namespace agea
