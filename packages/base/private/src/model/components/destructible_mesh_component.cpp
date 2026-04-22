#include "packages/base/model/components/destructible_mesh_component.h"

#include "packages/base/model/assets/destructible_mesh_asset.h"

#include "core/level.h"

#include <global_state/global_state.h>
#include <physics/physics_system.h>
#include <physics/destructible_physics.h>

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

    auto* ps = glob::glob_state().get_physics_system();
    if (!ps)
    {
        return;
    }
    auto& dp = ps->destructibles();

    const bool broken = dp.is_broken(m_physics_handle);

    if (!broken)
    {
        // Keep the physics entry in sync with the authoring transform so
        // chunks spawn at the correct world origin when the object breaks.
        dp.set_world_transform(m_physics_handle, get_transform_matrix());
        return;
    }

    if (m_is_broken != broken)
    {
        m_is_broken = broken;
    }

    // Broken — force a render rebuild so the builder forwards chunk
    // transforms from Jolt or handles the transition / expiry.
    mark_render_dirty();
}

bool
destructible_mesh_component::shatter()
{
    if (!m_physics_handle.valid())
    {
        return false;
    }
    auto* ps = glob::glob_state().get_physics_system();
    if (!ps)
    {
        return false;
    }
    // Make sure the latest transform is visible before chunks spawn.
    ps->destructibles().set_world_transform(m_physics_handle, get_transform_matrix());
    bool r = ps->destructibles().shatter(m_physics_handle);
    if (r)
    {
        m_is_broken = true;
        mark_render_dirty();
    }
    return r;
}

bool
destructible_mesh_component::apply_damage(float amount)
{
    if (!m_physics_handle.valid())
    {
        return false;
    }
    auto* ps = glob::glob_state().get_physics_system();
    if (!ps)
    {
        return false;
    }
    physics::impact hit;
    hit.damage = amount;
    hit.point = glm::vec3(get_world_position());
    ps->destructibles().set_world_transform(m_physics_handle, get_transform_matrix());
    bool r = ps->destructibles().apply_impact(m_physics_handle, hit);
    if (r)
    {
        m_is_broken = true;
        mark_render_dirty();
    }
    return r;
}

}  // namespace base
}  // namespace kryga
