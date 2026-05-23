#pragma once

#include "packages/base/model/directional_light_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

namespace kryga
{
namespace base
{
// clang-format off
KRG_ar_class(
    render_cmd_builder   = directional_light_component__cmd_builder,
    render_cmd_destroyer = directional_light_component__cmd_destroyer,
    mcp_hint             = "Directional light parameters — ambient/diffuse/specular colors and "
                           "light direction vector"
);
class directional_light_component : public ::kryga::root::game_object_component
// clang-format on
{
    // clang-format on
    KRG_gen_meta__directional_light_component();

public:
    KRG_gen_class_meta(directional_light_component, ::kryga::root::game_object_component);

    KRG_gen_construct_params{};
    KRG_gen_meta_api;

protected:
    // clang-format off
    KRG_ar_property(
        category     = "Light Properties",
        access       = all,
        serializable = true,
        invalidates  = render,
        mcp_hint     = "ambient light color RGB [0-1]"
    );
    ::kryga::root::vec3 m_ambient = glm::vec3{0.8f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Light Properties",
        access       = all,
        serializable = true,
        invalidates  = render,
        mcp_hint     = "diffuse light color RGB [0-1]"
    );
    ::kryga::root::vec3 m_diffuse = glm::vec3{0.1f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Light Properties",
        access       = all,
        serializable = true,
        invalidates  = render,
        mcp_hint     = "specular highlight color RGB [0-1]"
    );
    ::kryga::root::vec3 m_specular = glm::vec3{0.1f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Light Properties",
        access       = all,
        serializable = true,
        invalidates  = render,
        mcp_hint     = "light direction vector [normalized]"
    );
    ::kryga::root::vec3 m_direction = glm::vec3{1.0f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Light Properties",
        access       = all,
        serializable = true,
        mcp_hint     = "editor selection state for this light"
    );
    bool m_selected = false;
    // clang-format on
};

}  // namespace base
}  // namespace kryga
