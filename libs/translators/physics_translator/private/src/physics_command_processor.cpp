#include "physics_translator/physics_command_processor.h"

#include <physics/destructible_physics.h>
#include <physics/physics_system.h>

#include <utils/check.h>

namespace kryga
{

namespace
{
// Fixed physics timestep and the per-pump substep clamp. The clamp sheds backlog so
// a stall (debugger pause, frame hitch) can't trigger a spiral of death where each
// pump owes more steps than the last. Matches the decoupled-physics pattern in
// Unity/Unreal/Godot (fixed tick + max substeps).
constexpr float k_fixed_dt = 1.0f / 60.0f;
constexpr int k_max_substeps = 4;
}  // namespace

void
physics_command_processor::drain_commands()
{
    m_commands.drain([this](core::physics_message m) { apply(m); });
}

void
physics_command_processor::apply(const core::physics_message& m)
{
    auto& dp = m_ps.destructibles();

    const physics::destructible_handle h{m.handle};
    switch (m.kind)
    {
    case core::physics_msg_kind::register_destructible:
    {
        // Borrowed chunk pointer is dereferenced (and copied) right here, on the
        // physics thread, closing the borrow window opened at emit.
        dp.register_destructible(h, *m.chunks, m.damage_threshold);
        dp.set_lifetime(h, m.lifetime);
        dp.set_explosion_strength(h, m.explosion_strength);
        dp.set_world_transform(h, m.transform);
        m_active[m.handle] = static_cast<uint32_t>(m.chunks->size());
        break;
    }
    case core::physics_msg_kind::unregister_destructible:
        dp.unregister_destructible(h);
        m_active.erase(m.handle);
        break;
    case core::physics_msg_kind::set_transform:
        dp.set_world_transform(h, m.transform);
        break;
    case core::physics_msg_kind::apply_impact:
    {
        physics::impact hit;
        hit.point = m.impact_point;
        hit.impulse = m.impact_impulse;
        hit.damage = m.impact_damage;
        dp.apply_impact(h, hit);
        break;
    }
    case core::physics_msg_kind::shatter:
        dp.shatter(h);
        break;
    case core::physics_msg_kind::register_static_collider:
    {
        // The bridge minted m.handle and grew physics_system's storage at reserve();
        // here, on the physics thread, build the Jolt body and populate the slot that
        // handle indexes. The borrowed mesh pointer is dereferenced (copied) right
        // here, closing the borrow window.
        m_ps.create_static_mesh(physics::static_body_handle{m.handle}, *m.collider_mesh);
        break;
    }
    case core::physics_msg_kind::unregister_static_collider:
        m_ps.unregister_static_mesh(physics::static_body_handle{m.handle});
        break;
    }
}

bool
physics_command_processor::step(float dt)
{
    m_accumulator += dt;

    int steps = 0;
    while (m_accumulator >= k_fixed_dt && steps < k_max_substeps)
    {
        m_ps.tick(k_fixed_dt);
        m_accumulator -= k_fixed_dt;
        ++steps;
    }

    // Hit the clamp — drop the remaining backlog rather than chase it forever.
    if (steps == k_max_substeps && m_accumulator >= k_fixed_dt)
    {
        m_accumulator = 0.0f;
    }

    return steps > 0;
}

void
physics_command_processor::publish()
{
    auto& dp = m_ps.destructibles();

    for (const auto& [handle_value, chunk_count] : m_active)
    {
        const physics::destructible_handle h{handle_value};
        if (!dp.is_broken(h))
        {
            // Intact destructibles need no result — the model already knows.
            continue;
        }

        KRG_check(chunk_count <= core::k_max_result_chunks,
                  "destructible chunk count exceeds k_max_result_chunks "
                  "(raise the cap in core/physics_result.h)");

        core::physics_result r;
        r.handle = handle_value;
        r.broken = true;
        r.expired = dp.is_expired(h);
        r.chunk_count = chunk_count;
        for (uint32_t i = 0; i < chunk_count; ++i)
        {
            r.chunk_transforms[i] = dp.get_chunk_transform(h, i);
        }

        // try_push, NOT push: results are latest-wins, so dropping one on a full ring
        // is harmless (the next step republishes the current transforms). Crucially it
        // also keeps the worker from EVER blocking on the result ring — that, paired
        // with the command ring being blocking, would be a two-ring deadlock (model
        // blocked producing commands <-> physics blocked producing results). Transforms
        // ride inline now, so a dropped result owns no heap — nothing to free.
        m_results.try_push(std::move(r));
    }
}

void
physics_command_processor::process(float dt, uint32_t /*frame*/)
{
    drain_commands();

    if (m_paused.load(std::memory_order_relaxed))
    {
        return;
    }

    if (step(dt))
    {
        publish();
    }
}

}  // namespace kryga
