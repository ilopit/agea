#pragma once

#include "packages/base/model/terrain_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

#include <physics/physics_types.h>

namespace kryga
{
namespace root
{
class texture;
}  // namespace root

namespace base
{
class terrain_splatmap_material;

// Where the terrain heightfield comes from (value of m_source_mode).
constexpr uint32_t terrain_source_heightmap = 0u;  // sample red channel of m_heightmap
constexpr uint32_t terrain_source_noise = 1u;      // procedural fbm from noise params

// clang-format off
KRG_ar_class(
    render_cmd_builder   = terrain_component__cmd_builder,
    render_cmd_destroyer = terrain_component__cmd_destroyer,
    render_cmd_transform = terrain_component__cmd_transform,
    mcp_hint             = "Heightmap/noise terrain — generates a grid mesh and renders it with a "
                           "terrain_splatmap_material. Inherits transform from game_object_component"
);
class terrain_component : public ::kryga::root::game_object_component
// clang-format on
{
    KRG_gen_meta__terrain_component();

public:
    KRG_gen_class_meta(terrain_component, ::kryga::root::game_object_component);
    KRG_gen_construct_params{};
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
    get_local_centroid() const
    {
        return m_local_centroid;
    }
    void
    set_local_centroid(const glm::vec3& c)
    {
        m_local_centroid = c;
    }

    const physics::static_body_handle&
    get_physics_handle() const
    {
        return m_physics_handle;
    }
    void
    set_physics_handle(physics::static_body_handle h)
    {
        m_physics_handle = h;
    }

protected:
    // clang-format off
    KRG_ar_property(
        category     = "Assets",
        serializable = true,
        check        = not_same,
        invalidates  = render,
        access       = all,
        default      = true,
        instantiate  = share,
        mcp_hint     = "terrain_splatmap_material used to shade the surface"
    );
    ::kryga::base::terrain_splatmap_material* m_material = nullptr;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Terrain",
        serializable = true,
        invalidates  = render,
        access       = all,
        default      = true,
        mcp_hint     = "height source: 0 = heightmap texture / 1 = procedural noise"
    );
    uint32_t m_source_mode = 1u;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Assets",
        serializable = true,
        invalidates  = render,
        access       = all,
        default      = true,
        instantiate  = share,
        mcp_hint     = "grayscale heightmap texture — red channel is height; used when source is "
                       "heightmap"
    );
    ::kryga::root::texture* m_heightmap = nullptr;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Terrain",
        serializable = true,
        invalidates  = render,
        access       = all,
        default      = true,
        mcp_hint     = "terrain edge length in world units — square footprint"
    );
    float m_world_size = 100.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Terrain",
        serializable = true,
        invalidates  = render,
        access       = all,
        default      = true,
        mcp_hint     = "vertices per side — total verts = resolution^2. Higher = finer and heavier"
    );
    uint32_t m_resolution = 128u;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Terrain",
        serializable = true,
        invalidates  = render,
        access       = all,
        default      = true,
        mcp_hint     = "vertical scale — peak height in world units"
    );
    float m_height_scale = 20.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Noise",
        serializable = true,
        invalidates  = render,
        access       = all,
        default      = true,
        mcp_hint     = "noise RNG seed — deterministic"
    );
    uint32_t m_seed = 1337u;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Noise",
        serializable = true,
        invalidates  = render,
        access       = all,
        default      = true,
        mcp_hint     = "fbm octave count [1-8]"
    );
    uint32_t m_octaves = 5u;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Noise",
        serializable = true,
        invalidates  = render,
        access       = all,
        default      = true,
        mcp_hint     = "base noise frequency — features per terrain edge"
    );
    float m_frequency = 3.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Noise",
        serializable = true,
        invalidates  = render,
        access       = all,
        default      = true,
        mcp_hint     = "fbm lacunarity — frequency multiplier per octave ~2.0"
    );
    float m_lacunarity = 2.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Noise",
        serializable = true,
        invalidates  = render,
        access       = all,
        default      = true,
        mcp_hint     = "fbm gain — amplitude multiplier per octave ~0.5"
    );
    float m_gain = 0.5f;
    // clang-format on

    float m_base_bounding_radius = 0.0f;
    glm::vec3 m_local_centroid = {0.0f, 0.0f, 0.0f};
    physics::static_body_handle m_physics_handle{};
};

}  // namespace base
}  // namespace kryga
