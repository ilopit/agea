#include "packages/root/components/game_object_component.h"

#include "packages/root/game_object.h"

#include <core/level.h>

namespace agea
{
namespace root
{
const vec3 DEF_FORWARD(0, 0, -1);
const vec3 DEF_UP(0, 1, 0);
const vec3 DEF_RIGHT(1, 0, 0);

game_object_component::game_object_component()
{
}

game_object_component::~game_object_component()
{
}

bool
game_object_component::construct(construct_params& c)
{
    AGEA_return_false(base_class::construct(c));

    extract<vec3>(c.position, m_position);
    extract<vec3>(c.rotation, m_rotation);
    extract<vec3>(c.scale, m_scale);

    return true;
}

vec3
game_object_component::get_forward_vector() const
{
    return glm::rotate(glm::quat(m_rotation.as_glm()), DEF_FORWARD.as_glm());
}

vec3
game_object_component::get_up_vector() const
{
    return glm::rotate(glm::quat(m_rotation.as_glm()), DEF_UP.as_glm());
}

vec3
game_object_component::get_right_vector() const
{
    return glm::rotate(glm::quat(m_rotation.as_glm()), DEF_RIGHT.as_glm());
}

void
game_object_component::move(const vec3& delta)
{
    m_position += delta.as_glm();

    update_children_matrixes();

    mark_transform_dirty();
}

void
game_object_component::rotate(const vec3& delta)
{
    m_rotation += delta.as_glm();

    update_children_matrixes();

    mark_transform_dirty();
}

void
game_object_component::update_matrix()
{
    auto t = glm::translate(glm::mat4{1.0}, m_position);
    auto s = glm::scale(glm::mat4{1.0}, m_scale);

    auto rot_in_radians = glm::radians(m_rotation.as_glm());

    auto r = glm::toMat4(glm::quat(rot_in_radians));

    m_transform_matrix = t * s * r;
    m_world_position = glm::vec4(m_position, 1.0f);

    if (m_render_root)
    {
        m_transform_matrix = m_render_root->get_transofrm_matrix() * m_transform_matrix;
        m_world_position = m_render_root->get_transofrm_matrix() * s * r * m_world_position;
    }

    m_normal_matrix = glm::transpose(glm::inverse(m_transform_matrix));
}

void
game_object_component::mark_transform_dirty()
{
    if (!has_dirty_transform())
    {
        m_level->add_to_dirty_transform_queue(this);
        set_dirty_transform(true);
    }
}

void
game_object_component::mark_render_dirty()
{
    if (m_level && get_state() != smart_object_state::constructed)
    {
        m_level->add_to_dirty_render_queue(this);
        set_state(smart_object_state::constructed);
    }
}

void
game_object_component::update_children_matrixes()
{
    if (!get_owner())
    {
        return;
    }

    auto r = get_owner()->get_components(get_order_idx());

    for (auto& obj : r)
    {
        if (auto goc = obj.as<root::game_object_component>())
        {
            goc->update_matrix();
        }
    }
}

}  // namespace root
}  // namespace agea
