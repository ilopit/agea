#include "model/components/game_object_component.h"

#include <vulkan_render_types/vulkan_render_data.h>
#include <model_global_api/render_api.h>

namespace agea
{
namespace model
{
const glm::vec3 DEF_FORWARD(0, 0, -1);
const glm::vec3 DEF_UP(0, 1, 0);
const glm::vec3 DEF_RIGHT(1, 0, 0);

game_object_component::game_object_component()
{
    m_visible = &m_render_data->visible;
    m_renderable = &m_render_data->rendarable;
}

game_object_component::~game_object_component()
{
}

void
game_object_component::register_for_rendering()
{
    AGEA_check(m_render_data, "Should be valid");

    if (m_render_data->rendarable)
    {
        if (!m_render_data->empty())
        {
            glob::model_render_api::get()->invalidate(this);
        }
        else
        {
            auto id = m_render_data->gen_render_data_id();

            glob::model_render_api::get()->add_to_render_queue(this);
        }
    }

    for (auto c : m_render_components)
    {
        c->register_for_rendering();
    }
}

bool
game_object_component::prepare_for_rendering()
{
    for (auto rc : m_render_components)
    {
        rc->prepare_for_rendering();
    }

    m_dirty = false;
    m_owner_id = m_id;

    return true;
}

void
game_object_component::update_matrix()
{
    auto t = glm::translate(glm::mat4{1.0}, m_position);
    auto s = glm::scale(glm::mat4{1.0}, m_scale);

    auto rot_in_radians = glm::radians(m_rotation);

    auto r = glm::toMat4(glm::quat(rot_in_radians));

    m_render_data->transform_matrix = t * s * r;

    if (m_render_root)
    {
        m_render_data->transform_matrix =
            m_render_root->get_transofrm_matrix();  // *m_render_data->transform_matrix;
    }

    for (auto c : m_render_components)
    {
        c->update_matrix();
    }
}

glm::mat4
game_object_component::get_transofrm_matrix()
{
    return m_render_data->transform_matrix;
}

void
game_object_component::editor_update()
{
    if (m_render_data->rendarable)
    {
        mark_dirty();
    }
}

}  // namespace model
}  // namespace agea
