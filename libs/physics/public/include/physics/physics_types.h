#pragma once

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <vector>

namespace kryga
{
namespace physics
{

// 8-bit kind tag for static-collider handles. One identity space, shared across
// the split: physics_bridge owns the lane_allocator (mints on the model thread),
// physics_system owns the laned_storage<JPH::BodyID> the handle indexes (populated
// on the physics thread) — the render_translator/loader split applied to physics.
// Distinct from render's resource_kind (0-7) and the destructible kind (64) so a
// raw u64 can't be mistaken for a handle of another pool.
constexpr uint8_t k_static_collider_kind = 65;

struct chunk_shape
{
    glm::vec3 aabb_min{0.0f};
    glm::vec3 aabb_max{0.0f};
    glm::vec3 seed_point{0.0f};
    std::vector<glm::vec3> hull_points;
};

struct impact
{
    glm::vec3 point{0.0f};
    glm::vec3 impulse{0.0f};
    float damage = 0.0f;
};

struct destructible_handle
{
    uint64_t value = 0;

    bool
    valid() const
    {
        return value != 0;
    }
};

// Handle to an independently-registered static triangle-mesh collider
// (e.g. terrain). Additive: distinct from the single build_static_world body.
struct static_body_handle
{
    uint64_t value = 0;

    bool
    valid() const
    {
        return value != 0;
    }
};

// Triangle mesh in world-space, contributed to the static collision world.
// The caller flattens model-space vertices through their component transform
// before passing them in.
struct static_world_mesh
{
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;
};

}  // namespace physics
}  // namespace kryga
