#pragma once

#include <core/physics_message.h>

#include <physics/physics_types.h>

#include <utils/laned_pool.h>

#include <error_handling/error_handling.h>

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <vector>

// Forward-declared so this model-thread header never pulls Jolt: the static-collider
// allocator only holds a laned_storage<…, JPH::BodyID>* dispatch token (incomplete T
// is fine — it's never dereferenced here; physics_system populates the BodyIDs).
namespace JPH
{
class BodyID;
}

namespace kryga
{
namespace root
{
class smart_object;
class game_object_component;
}  // namespace root

namespace physics
{
class physics_system;
}

// 8-bit type tag for destructible identity handles. Model-side only: the payload
// lives in m_states here, never in render storage, so the value only keeps a raw
// u64 from being mistaken for a handle of another pool. The static-collider kind
// (physics::k_static_collider_kind) lives in physics_types.h — it's shared with
// physics_system, which owns the BodyID storage those handles index.
constexpr uint8_t k_destructible_handle_kind = 64;

// Model-thread snapshot of one destructible's physics state, refreshed by
// drain_results from the physics->model result ring. The destructible component
// and its render builder READ this (broken / expired / chunk transforms) instead
// of calling physics_system across the thread boundary — the read-back analog of
// how the model reads its own caches rather than the GPU.
struct destructible_state
{
    bool broken = false;
    bool expired = false;
    std::vector<glm::mat4> chunk_transforms;  // world-space, per chunk index
};

// The model<->physics analog of render_translator / audio_bridge: the model-thread
// PRODUCER for the command ring AND the model-thread CONSUMER for the result ring.
// Components never touch physics_system or the rings directly — they call these
// methods; the bridge pushes intents and owns the per-handle state snapshot.
// Translating drained COMMANDS into physics_system calls is NOT here; it lives in
// physics_command_processor (the physics-thread consumer), mirroring how
// render_translator builds commands while render_cmd::dispatch executes them.
//
// Two producer faces feed the SAME ring:
//   - destructibles call the typed methods below directly (register_destructible …);
//   - static colliders (terrain) are produced through the reflection dispatch
//     (physics_cmd_build/destroy/transform), the model→physics counterpart of
//     render_translator's per-type handlers, which call register_static_collider.
// Both ultimately emit() a core::physics_message — there is no second, main-thread
// command channel (the old arena/input_queue path was main-thread-only and unsafe
// once physics moved to its own thread).
//
// Threading: every method runs on the model/main thread. State (the handle minters)
// is touched solely from there.
class physics_bridge
{
public:
    // Binds the destructible identity allocator to its own (bridge-owned) state
    // storage, lane 0. Both live here on the model thread, so the claim is direct.
    physics_bridge();

    // Releases the destructible allocator's lane before m_states is destroyed
    // (the storage dtor asserts no allocator is still attached). The static-collider
    // allocator is released earlier, via detach_storages(), since its storage lives
    // in physics_system and is torn down on the engine's schedule.
    ~physics_bridge();

    // [shutdown, model thread] Release the static-collider allocator's lane on
    // physics_system's BodyID storage before that storage is destroyed. Mirrors
    // render_translator::detach_content_storages; call once before physics_system
    // shutdown/teardown.
    void
    detach_storages();

    // --- Reflection-dispatched producers (model thread) ---
    //
    // The physics twin of render_translator's render_cmd_build/destroy/transform:
    // walk a smart_object's per-type physics handler (if any) and let it emit the
    // right intents. A type with no physics handler is a no-op, so only physics
    // types (terrain, …) produce commands.

    kryga::result_code
    physics_cmd_build(root::smart_object& obj, bool sub_objects);

    kryga::result_code
    physics_cmd_destroy(root::smart_object& obj, bool sub_objects);

    kryga::result_code
    physics_cmd_transform(root::game_object_component& source);

    // --- Static collider producers (model thread) ---

    // Mint a static-collider identity, emit a register intent carrying a BORROWED
    // pointer to `mesh` (the processor copies it through create_static_mesh on the
    // physics thread — same borrow contract as register_destructible's chunks). The
    // identity is minted here (model-thread allocator) so the caller records it
    // synchronously without a round-trip to physics_system across the thread
    // boundary; the processor maps this identity to the real physics body handle.
    physics::static_body_handle
    register_static_collider(const physics::static_world_mesh& mesh);

    void
    unregister_static_collider(physics::static_body_handle h);

    // [init, model thread] Claim lane 0 of physics_system's BodyID storage for the
    // static-collider allocator. Mirrors render_translator::bind_content_storages:
    // the allocator mints handles that index that storage; growth is consumer-side
    // (physics thread). Call once after physics_system::init(), before registration.
    void
    bind_static_storage(physics::physics_system& ps);

    // --- Command producers (model thread) ---

    // Mint a handle, record an (initially intact) snapshot for it, and emit a
    // register intent carrying a BORROWED pointer to `chunks` (the physics thread
    // copies them — see physics_message.h). The handle is minted here so
    // registration needs no synchronous round-trip, exactly like render
    // pre-reserving a slot handle before enqueuing its create command.
    physics::destructible_handle
    register_destructible(const std::vector<physics::chunk_shape>& chunks,
                          float damage_threshold,
                          float lifetime,
                          float explosion_strength,
                          const glm::mat4& world_transform);

    void
    unregister(physics::destructible_handle h);

    void
    set_transform(physics::destructible_handle h, const glm::mat4& world);

    void
    apply_impact(physics::destructible_handle h, const physics::impact& hit);

    void
    shatter(physics::destructible_handle h);

    // --- Result consumer (still model thread) ---

    // Drain the physics->model result ring into the per-handle snapshot. Latest-wins:
    // a newer result for a handle overwrites the older, and the superseded heap
    // transform vector is freed. Call once per frame, before the render builder runs.
    void
    drain_results();

    // Latest snapshot for a handle, or nullptr if the handle is unknown (never
    // registered, or already unregistered). Model thread only.
    const destructible_state*
    get_state(physics::destructible_handle h) const;

private:
    void
    emit(const core::physics_message& msg);

    using alloc_handle = utils::handle<k_destructible_handle_kind>;

    // Destructible state storage + its identity allocator, BOTH model-thread, single
    // lane (lane 0), both owned here. The allocator mints dense indices + a generation
    // per index; the generation is load-bearing, not decoration: because indices
    // RECYCLE, a stale in-flight result for an unregistered handle fails valid()
    // instead of writing into the slot a newer destructible now owns. The storage
    // holds the per-handle snapshot the allocator's handle indexes (consumer-side
    // grow_for on register). m_states is declared FIRST so the ctor can claim its
    // lane and the dtor (after detach) can let it die last.
    utils::laned_storage<k_destructible_handle_kind, destructible_state> m_states{1};
    utils::lane_allocator<k_destructible_handle_kind, destructible_state> m_alloc;

    // Static-collider allocator, claims lane 0 of physics_system's BodyID storage via
    // bind_static_storage at init: the minted handle indexes that storage directly.
    // The render split — allocator here (model thread), storage in the system (physics
    // thread, consumer-side growth). One identity space, so the processor no longer
    // maps a bridge id to a separate physics-body handle.
    utils::lane_allocator<physics::k_static_collider_kind, JPH::BodyID> m_static_alloc;
};

}  // namespace kryga
