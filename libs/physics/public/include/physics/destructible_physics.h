#pragma once

#include <physics/physics_types.h>

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace kryga
{
namespace physics
{

class physics_system;

// Runtime state of destructible meshes registered with the physics world.
//
// Lifecycle expected from callers:
//   1. On component build: register_destructible(chunks, damage_threshold) and
//      keep the returned handle. Set world transform / lifetime / explosion
//      strength before the object can be hit.
//   2. Each frame while the owning component is intact: call
//      set_world_transform() so chunks spawn at the right origin when the
//      object breaks. Cheap while unbroken (no Jolt bodies exist yet).
//   3. Gameplay calls apply_impact() (accumulates damage) or shatter()
//      (force break). The transition to broken spawns one rigid body per
//      chunk; subsequent calls are no-ops.
//   4. Once broken, get_chunk_transform(i) returns the world transform of
//      chunk i, fed by Jolt integration.
//   5. is_expired() goes true once lifetime has elapsed since break. The
//      owning component is responsible for destroying its render objects
//      and then calling unregister_destructible().
class destructible_physics
{
    friend class physics_system;

public:
    explicit destructible_physics(physics_system& owner);
    ~destructible_physics();

    destructible_handle
    register_destructible(const std::vector<chunk_shape>& chunks, float damage_threshold);

    void
    unregister_destructible(destructible_handle h);

    void
    set_world_transform(destructible_handle h, const glm::mat4& world_mat);

    void
    set_lifetime(destructible_handle h, float seconds);

    void
    set_explosion_strength(destructible_handle h, float strength);

    // Applies an impact. Returns true iff this call caused the transition to
    // the broken state. Returns false if the object is already broken, or if
    // accumulated damage is still below threshold.
    bool
    apply_impact(destructible_handle h, const impact& hit);

    // Force break, bypassing damage accumulation. Returns true iff the object
    // just transitioned to broken.
    bool
    shatter(destructible_handle h);

    bool
    is_broken(destructible_handle h) const;

    bool
    is_expired(destructible_handle h) const;

    // World-space transform of chunk at index. Returns identity before break.
    glm::mat4
    get_chunk_transform(destructible_handle h, uint32_t chunk_index) const;

private:
    // Stepped from physics_system::tick(). Advances per-destructible lifetime
    // timers after break. Body integration is handled by Jolt's Update().
    void
    tick(float dt);

    struct impl;
    std::unique_ptr<impl> m_impl;
};

}  // namespace physics
}  // namespace kryga
