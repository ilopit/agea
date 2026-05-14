#pragma once

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace kryga
{
namespace physics
{

class destructible_physics;

// Triangle mesh in world-space, contributed to the static collision world.
// The caller flattens model-space vertices through their component transform
// before passing them in.
struct static_world_mesh
{
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;
};

class physics_system
{
    // destructible_physics needs to access the Jolt world stored in impl; both
    // types live in the same library, so we grant it friend access rather than
    // exposing Jolt types in this public header.
    friend class destructible_physics;

public:
    physics_system();
    ~physics_system();

    // Initialises Jolt globals + the physics world. Safe to call once per
    // process; subsequent calls after shutdown() re-initialise.
    void
    init();

    // Tears down the physics world and Jolt globals.
    void
    shutdown();

    void
    set_gravity(const glm::vec3& g);

    // Builds a single static body from the concatenation of all passed meshes.
    // Replaces any previously registered static world.
    void
    build_static_world(const std::vector<static_world_mesh>& meshes);

    // Convenience: install an infinite ground plane at y = plane_y. Used as a
    // fallback before level geometry is available, or when a level has no
    // static collision.
    void
    build_ground_plane(float plane_y = 0.0f, float half_extent = 10000.0f);

    void
    clear_static_world();

    // Advance the simulation by dt seconds.
    void
    tick(float dt);

    destructible_physics&
    destructibles();

private:
    struct impl;
    std::unique_ptr<impl> m_impl;
    std::unique_ptr<destructible_physics> m_destructibles;
};

}  // namespace physics
}  // namespace kryga
