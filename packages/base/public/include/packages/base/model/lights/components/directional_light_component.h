#pragma once

#include "packages/base/model/directional_light_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

namespace kryga
{
namespace base
{
// clang-format off
KRG_ar_class(render_cmd_builder   = directional_light_component__cmd_builder,
              render_cmd_destroyer = directional_light_component__cmd_destroyer);
class directional_light_component : public ::kryga::root::game_object_component
{
    // clang-format on
    KRG_gen_meta__directional_light_component();

public:
    KRG_gen_class_meta(directional_light_component, ::kryga::root::game_object_component);

    KRG_gen_construct_params{};
    KRG_gen_meta_api;

protected:
    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_ambient = glm::vec3{0.8f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_diffuse = glm::vec3{0.1f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_specular = glm::vec3{0.1f};

    KRG_ar_property("category=Light Properties", "access=all", "serializable=true");
    ::kryga::root::vec3 m_direction = glm::vec3{1.0f};
};

}  // namespace base
}  // namespace kryga
