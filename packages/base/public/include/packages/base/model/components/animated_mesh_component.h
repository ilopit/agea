#pragma once

#include "packages/base/model/animated_mesh_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

#include <utils/buffer.h>
#include <utils/id.h>

#include <glm/vec3.hpp>

namespace kryga
{
namespace root
{
class material;
}  // namespace root

namespace base
{
// clang-format off
KRG_ar_class(
    render_cmd_builder   = animated_mesh_component__cmd_builder,
    render_cmd_destroyer = animated_mesh_component__cmd_destroyer,
    mcp_hint             = "Renders animated 3D mesh with skeletal animation — supports clip "
                           "playback / speed control / looping"
);
class animated_mesh_component : public ::kryga::root::game_object_component
// clang-format on
{
    KRG_gen_meta__animated_mesh_component();

public:
    KRG_gen_class_meta(animated_mesh_component, ::kryga::root::game_object_component);

    KRG_gen_construct_params
    {
        utils::buffer gltf;
        ::kryga::root::material* material_handle = nullptr;
    };
    KRG_gen_meta_api;

    bool
    construct(construct_params& c);

    float
    get_base_bounding_radius() const
    {
        return m_base_bounding_radius;
    }
    void
    set_base_bounding_radius(float r)
    {
        m_base_bounding_radius = r;
    }

    const glm::vec3&
    get_base_centroid() const
    {
        return m_base_centroid;
    }
    void
    set_base_centroid(const glm::vec3& c)
    {
        m_base_centroid = c;
    }

    const utils::id&
    get_animation_instance_id() const
    {
        return m_anim_instance_id;
    }
    void
    set_anim_instance_id(const utils::id& v)
    {
        m_anim_instance_id = v;
    }
    void
    set_skeleton_id(const utils::id& v)
    {
        m_skeleton_id = v;
    }

protected:
    // clang-format off
    KRG_ar_property(
        "category=Assets",
        "serializable=true",
        "invalidates=render",
        "access=all",
        "mcp_hint=embedded glTF model binary — read-only at runtime"
    );
    utils::buffer m_gltf;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=Assets",
        "serializable=true",
        "invalidates=render",
        "access=all",
        "default=true",
        "mcp_hint=surface appearance: colors / textures / shading — inspect/edit via kryga_model_get_all with the material ID"
    );
    ::kryga::root::material* m_material = nullptr;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=Animation",
        "serializable=true",
        "access=all",
        "mcp_hint=name of the animation clip to play from the glTF"
    );
    utils::id m_clip_name;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=Animation",
        "serializable=true",
        "access=all",
        "default=true",
        "mcp_hint=animation speed multiplier: 1.0 = normal / 0.5 = half speed"
    );
    float m_playback_speed = 1.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=Animation",
        "serializable=true",
        "access=all",
        "default=true",
        "mcp_hint=whether the animation restarts when it reaches the end"
    );
    bool m_looping = true;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=Animation",
        "serializable=true",
        "access=all",
        "default=true",
        "mcp_hint=whether the animation is currently playing"
    );
    bool m_playing = true;
    // clang-format on

    // Runtime state (not serialized)
    float m_base_bounding_radius = 0.0f;
    glm::vec3 m_base_centroid{0.0f};
    utils::id m_anim_instance_id;
    utils::id m_skeleton_id;
};

}  // namespace base
}  // namespace kryga
