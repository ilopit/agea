#pragma once

#include "packages/base/model/destructible_mesh_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

#include <physics/destructible_physics.h>
#include <physics/physics_types.h>

#include <utils/id.h>
#include <render_types/render_handle.h>

#include <vector>

namespace kryga
{
namespace base
{
class destructible_mesh_asset;

// Destructible mesh component.
//
// Mirrors mesh_component but references a destructible_mesh_asset. Runtime
// flow:
//   - First cmd_build: pre-fracture source mesh into chunks (stored in
//     m_chunk_shapes), upload each chunk vertex/index buffer as a render
//     mesh resource (m_chunk_mesh_ids), register with physics.
//   - While unbroken: renders the source mesh; on_tick pushes the current
//     world transform into physics so chunks spawn at the right origin
//     when the object breaks.
//   - On break (apply_impact/shatter): cmd_build destroys the source render
//     object and creates one render object per chunk (m_chunk_render_ids).
//     on_tick marks render dirty each frame to forward chunk transforms
//     from Jolt into update_object_cmds.
//   - On expiry (lifetime elapsed since break): cmd_build destroys all
//     chunk render objects + their mesh resources, unregisters from
//     physics, and flips m_disposed so subsequent builds are no-ops.
// clang-format off
KRG_ar_class(
    render_cmd_builder   = destructible_mesh_component__cmd_builder,
    render_cmd_destroyer = destructible_mesh_component__cmd_destroyer
);
class destructible_mesh_component : public ::kryga::root::game_object_component
// clang-format on
{
    KRG_gen_meta__destructible_mesh_component();

public:
    KRG_gen_class_meta(destructible_mesh_component, ::kryga::root::game_object_component);

    KRG_gen_construct_params
    {
        destructible_mesh_asset* asset_handle = nullptr;
    };
    KRG_gen_meta_api;

    bool
    construct(construct_params& c);

    // Override game_object_component::on_tick — syncs physics world
    // transform while intact and forwards chunk transforms to render
    // after break.
    void
    on_tick(float dt) override;

    // Gameplay entry: force this destructible into the broken state.
    // Returns true iff this call caused the transition; subsequent calls
    // on an already-broken object return false.
    // clang-format off
    KRG_ar_function(
        category = "gameplay"
    );
    bool
    shatter();
    // clang-format on

    // Gameplay entry: apply damage. Returns true iff this call caused the
    // break. Use shatter() for "instant break" and apply_damage() for
    // accumulation against the asset's damage threshold.
    // clang-format off
    KRG_ar_function(
        category = "gameplay"
    );
    bool
    apply_damage(float amount);
    // clang-format on

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

    const physics::destructible_handle&
    get_physics_handle() const
    {
        return m_physics_handle;
    }
    void
    set_physics_handle(physics::destructible_handle h)
    {
        m_physics_handle = h;
    }

    bool
    get_is_broken() const
    {
        return m_is_broken;
    }
    void
    set_is_broken(bool v)
    {
        m_is_broken = v;
    }

    std::vector<physics::chunk_shape>&
    get_chunk_shapes()
    {
        return m_chunk_shapes;
    }
    const std::vector<physics::chunk_shape>&
    get_chunk_shapes() const
    {
        return m_chunk_shapes;
    }

    std::vector<utils::id>&
    get_chunk_mesh_ids()
    {
        return m_chunk_mesh_ids;
    }
    const std::vector<utils::id>&
    get_chunk_mesh_ids() const
    {
        return m_chunk_mesh_ids;
    }

    // Handle model: per-chunk render slots (runtime, not serialized). Parallel to
    // m_chunk_mesh_ids; chunks draw by handle, the id is kept for destroy.
    std::vector<render::types::mesh_handle>&
    get_chunk_mesh_handles()
    {
        return m_chunk_mesh_handles;
    }

    std::vector<utils::id>&
    get_chunk_render_ids()
    {
        return m_chunk_render_ids;
    }
    const std::vector<utils::id>&
    get_chunk_render_ids() const
    {
        return m_chunk_render_ids;
    }

    // Handle model: per-chunk render-object slots (runtime, not serialized).
    // Parallel to m_chunk_render_ids; chunks draw/update/destroy by handle, the
    // id is kept only as the create_object_cmd passenger (lightmap/diagnostics).
    std::vector<render::types::render_object_handle>&
    get_chunk_render_handles()
    {
        return m_chunk_render_handles;
    }

    bool
    get_chunks_rendering() const
    {
        return m_chunks_rendering;
    }
    void
    set_chunks_rendering(bool v)
    {
        m_chunks_rendering = v;
    }

    bool
    get_disposed() const
    {
        return m_disposed;
    }
    void
    set_disposed(bool v)
    {
        m_disposed = v;
    }

protected:
    // clang-format off
    KRG_ar_property(
        category     = "Assets",
        serializable = true,
        check        = not_same,
        invalidates  = render,
        access       = all,
        default      = true
    );
    destructible_mesh_asset* m_asset = nullptr;
    // clang-format on

    float m_base_bounding_radius = 0.0f;
    bool m_is_broken = false;
    bool m_chunks_rendering = false;
    bool m_disposed = false;
    physics::destructible_handle m_physics_handle{};
    std::vector<physics::chunk_shape> m_chunk_shapes;
    std::vector<utils::id> m_chunk_mesh_ids;
    std::vector<render::types::mesh_handle> m_chunk_mesh_handles;  // runtime, not serialized
    std::vector<utils::id> m_chunk_render_ids;
    std::vector<render::types::render_object_handle> m_chunk_render_handles;  // runtime
};

}  // namespace base
}  // namespace kryga
