#pragma once

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <vector>

namespace kryga
{
namespace physics
{
struct chunk_shape;
struct static_world_mesh;
}

namespace core
{

enum class physics_msg_kind : uint8_t
{
    register_destructible,
    unregister_destructible,
    set_transform,
    apply_impact,
    shatter,
    register_static_collider,
    unregister_static_collider,
};

// Model-emitted physics intent, pushed onto subsystem_queues().physics.in (via
// physics_translator) and translated into physics_system calls by
// physics_command_processor on the physics thread. The destructible component
// never calls physics_system directly — it pushes one of these, mirroring how
// the model marks render dirty instead of touching the GPU and how audio
// emitters push audio_message instead of touching audio_renderer.
//
// POD by design: it crosses the model->physics thread boundary BY VALUE through
// a lock-free SPSC ring, so core needs the complete type — but core must NOT
// depend on the physics lib (physics links core; that would cycle). So the
// destructible handle is a plain uint64 (mirrors physics::destructible_handle)
// and the chunk shapes are a raw, model-owned pointer (forward-declared).
//
// Pointer-lifetime hazard (register_destructible only): the physics thread
// dereferences `chunks` when it processes the message, LATER than (and on a
// different thread from) the emit. Safe in practice because chunks live in the
// owning component's m_chunk_shapes (a package-lifetime object) and the
// processor COPIES them into its entry on register, so the borrow window is one
// frame. Same hazard class as audio_message::clip — revisit if components
// become hot-deletable mid-frame.
struct physics_message
{
    physics_msg_kind kind = physics_msg_kind::shatter;
    uint64_t handle = 0;

    // register_destructible: borrowed pointer into the component's chunk shapes
    // (copied by the processor), plus the per-destructible parameters folded in
    // so registration is a single message.
    const std::vector<physics::chunk_shape>* chunks = nullptr;
    float damage_threshold = 100.0f;
    float lifetime = 3.0f;
    float explosion_strength = 8.0f;

    // set_transform + register_destructible: world transform to sync.
    glm::mat4 transform{1.0f};

    // apply_impact only.
    glm::vec3 impact_point{0.0f};
    glm::vec3 impact_impulse{0.0f};
    float impact_damage = 0.0f;

    // register_static_collider only: borrowed pointer into the owning component's
    // persistent collider mesh (e.g. terrain_component::collider_mesh). Same
    // borrow-window contract as `chunks` above — the component owns it for its
    // lifetime and the processor copies it through create_static_mesh on register,
    // so the model thread must not overwrite it before that drain (terrain
    // registers exactly once per handle, so it doesn't).
    const physics::static_world_mesh* collider_mesh = nullptr;
};

}  // namespace core
}  // namespace kryga
