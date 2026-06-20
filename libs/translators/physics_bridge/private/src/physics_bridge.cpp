#include "physics_bridge/physics_bridge.h"

#include <core/subsystem_queues.h>
#include <core/reflection/reflection_type.h>

#include <global_state/global_state.h>

#include <physics/physics_types.h>
#include <physics/physics_system.h>

#include <packages/root/model/smart_object.h>
#include <packages/root/model/components/game_object_component.h>
#include <packages/root/model/game_object.h>

namespace kryga
{

void
state_mutator__physics_bridge::set(gs::state& s)
{
    auto p = s.create_box<physics_bridge>("physics_bridge");
    s.m_physics_bridge = p;
}

physics_bridge::physics_bridge()
{
    // Claim lane 0 of our own state storage. Direct: allocator and storage share
    // this (the model) thread, so no queued release is needed at teardown.
    m_alloc.bind(m_states, 0);
}

physics_bridge::~physics_bridge()
{
    // Release the destructible allocator's lane before m_states is destroyed; the
    // storage dtor asserts no allocator is still attached. detach_storages() has
    // already released the static-collider lane (its storage lives elsewhere).
    if (m_alloc.bound())
    {
        m_alloc.detach();
    }
}

void
physics_bridge::detach_storages()
{
    if (m_static_alloc.bound())
    {
        m_static_alloc.detach();
    }
}

kryga::result_code
physics_bridge::physics_cmd_build(root::smart_object& obj, bool sub_objects)
{
    // CDOs are shared, readonly templates — never carry physics bodies.
    if (obj.get_flags().default_obj)
    {
        return result_code::ok;
    }

    auto build_fn = obj.get_reflection()->physics_cmd_builder;
    if (!build_fn)
    {
        return result_code::ok;
    }

    reflection::type_context__physics_cmd_build ctx{this, &obj, sub_objects};
    return build_fn(ctx);
}

kryga::result_code
physics_bridge::physics_cmd_destroy(root::smart_object& obj, bool sub_objects)
{
    if (obj.get_flags().default_obj)
    {
        return result_code::ok;
    }

    auto destroy_fn = obj.get_reflection()->physics_cmd_destroyer;
    if (!destroy_fn)
    {
        return result_code::ok;
    }

    reflection::type_context__physics_cmd_build ctx{this, &obj, sub_objects};
    return destroy_fn(ctx);
}

kryga::result_code
physics_bridge::physics_cmd_transform(root::game_object_component& source)
{
    auto r = source.get_owner()->get_components(source.get_order_idx());

    for (auto& obj : r)
    {
        auto handler = obj.get_reflection()->physics_cmd_transform;
        if (!handler)
        {
            continue;
        }

        reflection::type_context__physics_cmd_build ctx{this, &obj};
        handler(ctx);
    }

    return result_code::ok;
}

physics::static_body_handle
physics_bridge::register_static_collider(const physics::static_world_mesh& mesh)
{
    const auto ah = m_static_alloc.reserve();

    physics::static_body_handle h;
    h.value = ah.v;

    core::physics_message msg;
    msg.kind = core::physics_msg_kind::register_static_collider;
    msg.handle = h.value;
    msg.collider_mesh = &mesh;  // borrowed; the processor copies on register
    emit(msg);

    return h;
}

void
physics_bridge::unregister_static_collider(physics::static_body_handle h)
{
    if (!h.valid())
    {
        return;
    }

    const utils::handle<physics::k_static_collider_kind> ah{h.value};
    if (m_static_alloc.valid(ah))
    {
        m_static_alloc.reclaim(ah);
    }

    core::physics_message msg;
    msg.kind = core::physics_msg_kind::unregister_static_collider;
    msg.handle = h.value;
    emit(msg);
}

void
physics_bridge::bind_static_storage(physics::physics_system& ps)
{
    m_static_alloc.bind(ps.static_collider_storage(), 0);
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
    // Consumer-side growth on our own thread: make the slot addressable, mark it live
    // for this handle's generation, and reset any recycled slot's stale payload.
    m_states.grow_for(ah);
    m_states.set_generation(ah, ah.generation());
    *m_states.at(ah) = destructible_state{};

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
        m_states.reset(ah);  // clear payload + shadow gen 0
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

            auto& st = *m_states.at(ah);
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
    return m_states.at(ah);
}

}  // namespace kryga
