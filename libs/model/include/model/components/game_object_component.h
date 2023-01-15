#pragma once

#include "game_object_component.generated.h"

#include "model/components/component.h"
#include "model/core_types/vec3.h"

namespace agea
{

namespace render
{
class object_data;
}

namespace model
{

const extern vec3 DEF_FORWARD;
const extern vec3 DEF_UP;
const extern vec3 DEF_RIGHT;

AGEA_class();
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

    void
    update_matrix();

    glm::mat4
    get_transofrm_matrix();

    glm::mat4
    get_normal_matrix();

    glm::vec4
    get_world_position();

    virtual void
    on_tick(float dt);

    std::vector<game_object_component*>&
    get_render_components()
    {
        return m_render_components;
    }

    render::object_data*
    get_object_dat()
    {
        return m_render_handle;
    }

    void
    set_object_dat(render::object_data* v)
    {
        m_render_handle = v;
    }

    void
    mark_transform_dirty();

    void
    mark_render_dirty();

protected:
    AGEA_property("category=world", "serializable=true", "hint=x,y,z", "default=true");
    vec3 m_position = {0.f};

    AGEA_property("category=world", "serializable=true", "hint=x,y,z", "default=true");
    vec3 m_rotation = {0.f};

    AGEA_property("category=world", "serializable=true", "hint=x,y,z", "default=true");
    vec3 m_scale = {1.f};

    glm::mat4 m_transform_matrix;
    glm::mat4 m_normal_matrix;
    glm::vec4 m_world_position;

    std::vector<game_object_component*> m_render_components;
    game_object_component* m_render_root = nullptr;
    render::object_data* m_render_handle = nullptr;
};

}  // namespace model
}  // namespace agea
