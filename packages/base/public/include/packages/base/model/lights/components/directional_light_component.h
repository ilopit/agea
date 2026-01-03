#pragma once

#include "packages/base/model/directional_light_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

namespace kryga
{
namespace render
{
class vulkan_directional_light_data;
}

namespace base
{
// clang-format off
KRG_ar_class(render_constructor = directional_light_component__render_loader,
              render_destructor  = directional_light_component__render_destructor);
class directional_light_component : public ::kryga::root::game_object_component
{
    // clang-format on
    KRG_gen_meta__directional_light_component();

public:
    KRG_gen_class_meta(directional_light_component, ::kryga::root::game_object_component);

    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    void
    set_handler(render::vulkan_directional_light_data* h)
    {
        m_handler = h;
    }

    render::vulkan_directional_light_data*
    get_handler()
    {
        return m_handler;
    }

protected:
    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_ambient = glm::vec3{0.1f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_diffuse = glm::vec3{0.1f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_specular = glm::vec3{0.1f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_direction = glm::vec3{1.0f};

    render::vulkan_directional_light_data* m_handler = nullptr;
};

}  // namespace base
}  // namespace kryga
