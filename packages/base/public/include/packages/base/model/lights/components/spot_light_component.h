#pragma once

#include "packages/base/model/spot_light_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

namespace kryga
{
namespace base
{

// clang-format off
KRG_ar_class(
    render_cmd_builder   = spot_light_component__cmd_builder,
    render_cmd_destroyer = spot_light_component__cmd_destroyer,
    render_cmd_transform = spot_light_component__cmd_transform,
    mcp_hint             = "Spot light parameters — colors / direction / radius and inner/outer "
                           "cone angles in degrees"
);
class spot_light_component : public ::kryga::root::game_object_component
// clang-format on
{
    KRG_gen_meta__spot_light_component();

public:
    KRG_gen_class_meta(spot_light_component, game_object_component);

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
    ::kryga::root::vec3 m_ambient = glm::vec3{1.0f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Light Properties",
        access       = all,
        serializable = true,
        invalidates  = render,
        mcp_hint     = "diffuse light color RGB [0-1]"
    );
    ::kryga::root::vec3 m_diffuse = glm::vec3{1.0f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Light Properties",
        access       = all,
        serializable = true,
        invalidates  = render,
        mcp_hint     = "spot light aim direction [normalized]"
    );
    ::kryga::root::vec3 m_direction = glm::vec3{1.0f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Light Properties",
        access       = all,
        serializable = true,
        invalidates  = render,
        mcp_hint     = "specular highlight color RGB [0-1]"
    );
    ::kryga::root::vec3 m_specular = glm::vec3{1.0f};
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Light Properties",
        access       = all,
        serializable = true,
        invalidates  = render,
        mcp_hint     = "how far the light reaches in world units"
    );
    float m_radius = 50.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Light Properties",
        access       = all,
        serializable = true,
        invalidates  = render,
        mcp_hint     = "inner cone angle in degrees — full brightness inside this"
    );
    float m_cut_off = 17.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Light Properties",
        access       = all,
        serializable = true,
        invalidates  = render,
        mcp_hint     = "outer cone angle in degrees — light fades between cut_off and this"
    );
    float m_outer_cut_off = 18.f;
    // clang-format on

public:
    // Handle-model render slot (runtime, not serialized): the universal-light pool
    // slot reserved by the builder. Mirrors smart_object::m_render_object_handle.
    ::kryga::render::types::universal_light_handle m_render_light_handle = {};

    ::kryga::render::types::universal_light_handle
    get_render_light_handle() const
    {
        return m_render_light_handle;
    }

    void
    set_render_light_handle(::kryga::render::types::universal_light_handle h)
    {
        m_render_light_handle = h;
    }
};

}  // namespace base
}  // namespace kryga
