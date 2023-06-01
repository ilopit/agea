#pragma once

#include "root/directional_light_component.generated.h"

#include "root/components/game_object_component.h"

namespace agea
{
namespace render
{
class light_data;
}

namespace root
{

AGEA_ar_class();
class directional_light_component : public game_object_component
{
    AGEA_gen_meta__directional_light_component();

public:
    AGEA_gen_class_meta(directional_light_component, game_object_component);

    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    void
    set_handler(render::light_data* h)
    {
        m_handler = h;
    }

    render::light_data*
    get_handler()
    {
        return m_handler;
    }

protected:
    AGEA_ar_property("category=misc", "access=all", "serializable=true");
    glm::vec3 m_ambient = glm::vec3{0.1f};

    AGEA_ar_property("category=misc", "access=all", "serializable=true");
    glm::vec3 m_diffuse = glm::vec3{0.1f};

    AGEA_ar_property("category=misc", "access=all", "serializable=true");
    glm::vec3 m_specular = glm::vec3{0.1f};

    AGEA_ar_property("category=misc", "access=all", "serializable=true");
    glm::vec3 m_direction = glm::vec3{1.0f};

    render::light_data* m_handler = nullptr;
};

}  // namespace root
}  // namespace agea
