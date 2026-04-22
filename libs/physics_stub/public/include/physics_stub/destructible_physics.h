#pragma once

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <vector>

// Physics dummy for destructible meshes.
//
// This lib exists so destructible_mesh_component can be fully wired end-to-end
// without a real physics subsystem. The real implementation lands in a
// follow-up PR that replaces this lib (same header, real body) so call sites
// do not need to change.
//
// Contract the real implementation must preserve:
//   - register_destructible(id, chunk_aabbs) is called once per destructible
//     component on load.
//   - apply_impact() returns true iff the object just transitioned to broken.
//   - After broken, get_chunk_transform(idx) returns the current world-space
//     transform of chunk idx (in the object's local frame the transform is
//     initially identity).
//   - unregister_destructible() is called on component destroy.

namespace kryga
{
namespace physics_stub
{

struct chunk_shape
{
    glm::vec3 aabb_min{0.0f};
    glm::vec3 aabb_max{0.0f};
    glm::vec3 seed_point{0.0f};
};

struct impact
{
    glm::vec3 point{0.0f};
    glm::vec3 impulse{0.0f};
    float damage = 0.0f;
};

// Opaque handle — wraps a uint64_t so the real implementation can route it to
// a rigid-body pool without leaking its internals.
struct destructible_handle
{
    uint64_t value = 0;

    bool
    valid() const
    {
        return value != 0;
    }
};

class destructible_physics
{
public:
    destructible_physics();
    ~destructible_physics();

    destructible_handle
    register_destructible(const std::vector<chunk_shape>& chunks, float damage_threshold);

    void
    unregister_destructible(destructible_handle h);

    // Applies an impact to a registered destructible. Returns true iff this
    // call caused the object to transition to broken state. Subsequent calls
    // on an already-broken object return false.
    bool
    apply_impact(destructible_handle h, const impact& hit);

    bool
    is_broken(destructible_handle h) const;

    // Local-space transform of chunk at index. For unbroken objects this is
    // identity. The stub keeps this identity even after break.
    glm::mat4
    get_chunk_transform(destructible_handle h, uint32_t chunk_index) const;

private:
    struct impl;
    impl* m_impl = nullptr;
};

}  // namespace physics_stub
}  // namespace kryga
