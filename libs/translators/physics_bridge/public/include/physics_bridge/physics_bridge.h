#pragma once

#include <core/physics_message.h>

#include <physics/physics_types.h>

#include <render_types/handle_pool.h>

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <vector>

namespace kryga
{

// 8-bit type tag for the destructible identity handle. Distinct from render's
// resource_kind values (0-7) so a raw u64 can never be mistaken for a render
// handle; physics handles never route through render storage, so the exact value
// only matters for that disambiguation.
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
// The destructible component never touches physics_system or the rings directly —
// it calls these methods; the bridge pushes intents and owns the per-handle state
// snapshot. Translating drained COMMANDS into physics_system calls is NOT here; it
// lives in physics_command_processor (the physics-thread consumer), mirroring how
// render_translator builds commands while render_cmd::dispatch executes them.
//
// Threading: every method runs on the model/main thread. State (the handle minter
// and the snapshot map) is touched solely from there.
class physics_bridge
{
public:
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

    using alloc_handle = render::types::handle<k_destructible_handle_kind>;

    // Model-thread-only identity minter: a pure handle allocator (no slot_storage
    // grower — we keep the payload in m_states ourselves). Hands out dense indices
    // + a generation per index; the generation is load-bearing here, not decoration:
    // because indices RECYCLE, a stale in-flight result for an unregistered handle
    // fails valid() instead of writing into the slot a newer destructible now owns.
    render::types::handle_allocator<k_destructible_handle_kind> m_alloc;

    // Dense per-handle state, indexed by alloc_handle::index(). Entry reset on
    // register, read through get_state(), refreshed by drain_results. Replaces the
    // old handle->state hash map — the allocator already owns dense indices, so the
    // payload is a flat vector, not a map.
    std::vector<destructible_state> m_states;
};

}  // namespace kryga
