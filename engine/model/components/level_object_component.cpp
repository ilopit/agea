#include "level_object_component.h"

#include "vulkan_render/vulkan_render_data.h"
#include "vk_engine.h"

namespace agea
{
namespace model
{
const glm::vec3 DEF_FORWARD(0, 0, -1);
const glm::vec3 DEF_UP(0, 1, 0);
const glm::vec3 DEF_RIGHT(1, 0, 0);

bool
level_object_component::clone(this_class& src)
{
    AGEA_return_nok(base_class::clone(src));

    m_components.resize(src.m_components.size());

    for (auto i = 0; i < src.m_components.size(); ++i)
    {
        auto cloned = cast_ref<component>(src.m_components[i]->META_clone_obj());

        m_components[cloned->m_order_idx] = cloned;
    }

    m_position = src.m_position;
    m_rotation = src.m_rotation;
    m_scale = src.m_scale;

    return true;
}

void
level_object_component::gather_render_objcets()
{
    if (m_render_object && m_render_object->visible)
    {
        auto id = m_render_object->id();

        glob::engine::get()->add_to_rdc(id, m_render_object);
    }

    for (auto c : m_render_components)
    {
        c->gather_render_objcets();
    }
}

bool
level_object_component::prepare_for_rendering()
{
    m_render_object = new render_data();

    for (auto rc : m_render_components)
    {
        rc->prepare_for_rendering();
    }

    return true;
}

void
level_object_component::update_matrix()
{
    auto t = glm::translate(glm::mat4{1.0}, m_position);
    glm::mat4 s = glm::scale(glm::mat4{1.0}, m_scale);

    auto rot_in_radians = glm::radians(m_rotation);

    glm::mat4 r = glm::toMat4(glm::quat(rot_in_radians));

    m_render_object->transform_matrix = t * s * r;

    if (m_render_root)
    {
        m_render_object->transform_matrix =
            m_render_root->transofrm_matrix() * m_render_object->transform_matrix;
    }

    for (auto c : m_render_components)
    {
        c->update_matrix();
    }
}

glm::mat4
level_object_component::transofrm_matrix()
{
    return m_render_object->transform_matrix;
}

bool
level_object_component::deserialize_finalize(json_conteiner& c)
{
    AGEA_return_nok(base_class::deserialize_finalize(c));

    game_object_serialization_helper::read_3vec("position", c, "x", "y", "z", m_position.x,
                                                m_position.y, m_position.z);

    game_object_serialization_helper::read_3vec("scale", c, "x", "y", "z", m_scale.x, m_scale.y,
                                                m_scale.z);

    game_object_serialization_helper::read_3vec("rotation", c, "r", "p", "y", m_rotation.x,
                                                m_rotation.y, m_rotation.z);
    return true;
}

}  // namespace model
}  // namespace agea
