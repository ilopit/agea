#pragma once

#include "packages/base/model/destructible_mesh_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

#include <physics_stub/destructible_physics.h>

#include <vector>

namespace kryga
{
namespace base
{
class destructible_mesh_asset;

// Destructible mesh component.
//
// Mirrors the role of mesh_component but references a destructible_mesh_asset
// instead of a raw mesh+material pair. The component owns an opaque physics
// handle (currently backed by physics_stub; will be replaced by the real
// physics subsystem without changing this API).
//
// Render behaviour (v1 scaffolding):
//   - Unbroken: renders the source mesh of the referenced asset, identical
//     to a regular mesh_component.
//   - Broken: rendering is currently unchanged — chunk-mesh upload and
//     per-chunk render objects land together with the real physics PR, since
//     the per-chunk transforms come from the physics side.
// clang-format off
KRG_ar_class(render_cmd_builder   = destructible_mesh_component__cmd_builder,
              render_cmd_destroyer = destructible_mesh_component__cmd_destroyer);
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

    destructible_mesh_asset*
    get_asset() const
    {
        return m_asset;
    }

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

    const physics_stub::destructible_handle&
    get_physics_handle() const
    {
        return m_physics_handle;
    }
    void
    set_physics_handle(physics_stub::destructible_handle h)
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

    std::vector<physics_stub::chunk_shape>&
    get_chunk_shapes()
    {
        return m_chunk_shapes;
    }
    const std::vector<physics_stub::chunk_shape>&
    get_chunk_shapes() const
    {
        return m_chunk_shapes;
    }

protected:
    KRG_ar_property("category=Assets",
                    "serializable=true",
                    "check=not_same",
                    "invalidates=render",
                    "access=all",
                    "default=true");
    destructible_mesh_asset* m_asset = nullptr;

    float m_base_bounding_radius = 0.0f;
    bool m_is_broken = false;
    physics_stub::destructible_handle m_physics_handle{};
    std::vector<physics_stub::chunk_shape> m_chunk_shapes;
};

}  // namespace base
}  // namespace kryga
