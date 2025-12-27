#pragma once

#include "packages/base/model/spot_light_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

namespace agea
{
namespace render
{
class vulkan_spot_light_data;
}

namespace base
{

AGEA_ar_class(render_constructor = spot_light_component__render_loader,
              render_destructor = spot_light_component__render_destructor);
class spot_light_component : public ::agea::root::game_object_component
{
    AGEA_gen_meta__spot_light_component();

public:
    AGEA_gen_class_meta(spot_light_component, game_object_component);

    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    void
    set_handler(render::vulkan_spot_light_data* h)
    {
        m_handler = h;
    }

    render::vulkan_spot_light_data*
    get_handler()
    {
        return m_handler;
    }

protected:
    AGEA_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::agea::root::vec3 m_ambient = glm::vec3{1.0f};

    AGEA_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::agea::root::vec3 m_diffuse = glm::vec3{1.0f};

    AGEA_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::agea::root::vec3 m_direction = glm::vec3{1.0f};

    AGEA_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::agea::root::vec3 m_specular = glm::vec3{1.0f};

    AGEA_ar_property("category=Light Properties", "access=all", "serializable=true");
    float m_constant = 1.0f;

    AGEA_ar_property("category=Light Properties", "access=all", "serializable=true");
    float m_linear = 0.014f;

    AGEA_ar_property("category=Light Properties", "access=all", "serializable=true");
    float m_quadratic = 0.0007f;

    AGEA_ar_property("category=Light Properties", "access=all", "serializable=true");
    float m_cut_off = 17.0f;

    AGEA_ar_property("category=Light Properties", "access=all", "serializable=true");
    float m_outer_cut_off = 18.f;

    render::vulkan_spot_light_data* m_handler = nullptr;
};

}  // namespace base
}  // namespace agea
