#include "packages/base/model/components/destructible_mesh_component.h"

#include "packages/base/model/assets/destructible_mesh_asset.h"

#include "core/level.h"

#include <global_state/global_state.h>
#include <physics/physics_types.h>
#include <physics_bridge/physics_bridge.h>

namespace kryga
{
namespace base
{

KRG_gen_class_cd_default(destructible_mesh_component);

bool
destructible_mesh_component::construct(construct_params& c)
{
    KRG_return_false(base_class::construct(c));

    if (c.asset_handle)
    {
        m_asset = c.asset_handle;
    }

    // Must tick to drive the physics → render sync loop.
    m_tickable = true;

    return true;
}

void
destructible_mesh_component::on_tick(float /*dt*/)
{
    if (m_disposed)
    {
        return;
    }
    if (!m_physics_handle.valid())
    {
        return;
    }

    // Read the published physics snapshot through the bridge instead of querying the
    // Jolt world (now owned by the physics thread). No snapshot yet means the register
    // intent hasn't round-tripped — nothing to do this frame.
    auto& pb = glob::glob_state().getr_physics_bridge();
    const auto* st = pb.get_state(m_physics_handle);
    if (!st)
    {
        return;
    }

    if (!st->broken)
    {
        // Keep the physics entry in sync with the authoring transform so chunks spawn
        // at the correct world origin when the object breaks. Emitted as an intent;
        // applied on the physics thread.
        pb.set_transform(m_physics_handle, get_transform_matrix());
        return;
    }

    if (m_is_broken != st->broken)
    {
        m_is_broken = st->broken;
    }

    // Broken — force a render rebuild so the builder forwards the latest chunk
    // transforms from the snapshot or handles the transition / expiry.
    mark_render_dirty();
}

bool
destructible_mesh_component::shatter()
{
    if (!m_physics_handle.valid())
    {
        return false;
    }
    // Sync the latest transform first so chunks spawn at the right origin, then request
    // the break. Both ride the single command ring, so ordering is preserved. Now
    // ASYNC: the broken state reflects back via results 1-2 frames later (see on_tick),
    // so the return value means "request accepted", not "broke this instant".
    auto& pb = glob::glob_state().getr_physics_bridge();
    pb.set_transform(m_physics_handle, get_transform_matrix());
    pb.shatter(m_physics_handle);
    return true;
}

bool
destructible_mesh_component::apply_damage(float amount)
{
    if (!m_physics_handle.valid())
    {
        return false;
    }
    // Accumulation + the break decision happen on the physics thread; we can't know
    // synchronously whether this impact crossed the threshold. Returns "request
    // accepted" — the resulting break (if any) surfaces via the snapshot in on_tick.
    auto& pb = glob::glob_state().getr_physics_bridge();
    physics::impact hit;
    hit.damage = amount;
    hit.point = glm::vec3(get_world_position());
    pb.set_transform(m_physics_handle, get_transform_matrix());
    pb.apply_impact(m_physics_handle, hit);
    return true;
}

}  // namespace base
}  // namespace kryga
