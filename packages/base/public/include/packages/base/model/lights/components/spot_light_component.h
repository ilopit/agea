#pragma once

#include "packages/base/model/spot_light_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

namespace kryga
{
namespace render
{
class vulkan_universal_light_data;
}  // namespace render

namespace base
{

KRG_ar_class(render_constructor = spot_light_component__render_loader,
              render_destructor = spot_light_component__render_destructor);
class spot_light_component : public ::kryga::root::game_object_component
{
    KRG_gen_meta__spot_light_component();

public:
    KRG_gen_class_meta(spot_light_component, game_object_component);

    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    void
    set_handler(render::vulkan_universal_light_data* h)
    {
        m_handler = h;
    }

    render::vulkan_universal_light_data*
    get_handler()
    {
        return m_handler;
    }

protected:
    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_ambient = glm::vec3{1.0f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_diffuse = glm::vec3{1.0f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_direction = glm::vec3{1.0f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_specular = glm::vec3{1.0f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    float m_constant = 1.0f;

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    float m_linear = 0.014f;

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    float m_quadratic = 0.0007f;

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    float m_cut_off = 17.0f;

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    float m_outer_cut_off = 18.f;

    render::vulkan_universal_light_data* m_handler = nullptr;
};

}  // namespace base
}  // namespace kryga
