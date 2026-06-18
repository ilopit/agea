#include "physics_bridge/physics_bridge.h"

#include <core/subsystem_queues.h>

#include <global_state/global_state.h>

namespace kryga
{

void
state_mutator__physics_bridge::set(gs::state& s)
{
    auto p = s.create_box<physics_bridge>("physics_bridge");
    s.m_physics_bridge = p;
}

void
physics_bridge::emit(const core::physics_message& msg)
{
    // Value SPSC: copy the POD intent into the ring; the physics thread pops it and
    // feeds it to the processor. push() spin-blocks if full, but the queue is sized
    // well above the realistic per-frame intent count.
    glob::glob_state().getr_subsystem_queues().physics.in.push(core::physics_message(msg));
}

physics::destructible_handle
physics_bridge::register_destructible(const std::vector<physics::chunk_shape>& chunks,
                                      float damage_threshold,
                                      float lifetime,
                                      float explosion_strength,
                                      const glm::mat4& world_transform)
{
    const alloc_handle ah = m_alloc.reserve();
    const uint32_t idx = ah.index();
    if (idx >= m_states.size())
    {
        m_states.resize(idx + 1);
    }
    m_states[idx] = destructible_state{};  // reset a recycled slot's stale payload

    physics::destructible_handle h;
    h.value = ah.v;

    core::physics_message msg;
    msg.kind = core::physics_msg_kind::register_destructible;
    msg.handle = h.value;
    msg.chunks = &chunks;  // borrowed; the processor copies on register
    msg.damage_threshold = damage_threshold;
    msg.lifetime = lifetime;
    msg.explosion_strength = explosion_strength;
    msg.transform = world_transform;
    emit(msg);

    return h;
}

void
physics_bridge::unregister(physics::destructible_handle h)
{
    if (!h.valid())
    {
        return;
    }
    // Retire the identity (bumps the index's generation so any in-flight result for
    // it now fails valid()) and clear the slot, then tell physics to free its bodies.
    const alloc_handle ah{h.value};
    if (m_alloc.valid(ah))
    {
        m_states[ah.index()] = destructible_state{};
        m_alloc.reclaim(ah);
    }

    core::physics_message msg;
    msg.kind = core::physics_msg_kind::unregister_destructible;
    msg.handle = h.value;
    emit(msg);
}

void
physics_bridge::set_transform(physics::destructible_handle h, const glm::mat4& world)
{
    if (!h.valid())
    {
        return;
    }
    core::physics_message msg;
    msg.kind = core::physics_msg_kind::set_transform;
    msg.handle = h.value;
    msg.transform = world;
    emit(msg);
}

void
physics_bridge::apply_impact(physics::destructible_handle h, const physics::impact& hit)
{
    if (!h.valid())
    {
        return;
    }
    core::physics_message msg;
    msg.kind = core::physics_msg_kind::apply_impact;
    msg.handle = h.value;
    msg.impact_point = hit.point;
    msg.impact_impulse = hit.impulse;
    msg.impact_damage = hit.damage;
    emit(msg);
}

void
physics_bridge::shatter(physics::destructible_handle h)
{
    if (!h.valid())
    {
        return;
    }
    core::physics_message msg;
    msg.kind = core::physics_msg_kind::shatter;
    msg.handle = h.value;
    emit(msg);
}

void
physics_bridge::drain_results()
{
    auto& q = glob::glob_state().getr_subsystem_queues().physics.out;
    q.drain(
        [this](const core::physics_result& r)
        {
            const alloc_handle ah{r.handle};
            // Stale result for an unregistered / recycled handle — the generation
            // mismatch makes valid() false, so we simply drop it (latest-wins
            // reconciliation against a slot a newer destructible may now own).
            if (!m_alloc.valid(ah))
            {
                return;
            }

            auto& st = m_states[ah.index()];
            st.broken = r.broken;
            st.expired = r.expired;
            // assign() into the reused slot vector — keeps its capacity across
            // frames, so steady-state has no allocation on the result path either.
            st.chunk_transforms.assign(r.chunk_transforms, r.chunk_transforms + r.chunk_count);
        });
}

const destructible_state*
physics_bridge::get_state(physics::destructible_handle h) const
{
    const alloc_handle ah{h.value};
    if (!m_alloc.valid(ah))
    {
        return nullptr;
    }
    return &m_states[ah.index()];
}

}  // namespace kryga
