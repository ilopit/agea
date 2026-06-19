#pragma once

#include <glm_unofficial/glm.h>

#include <physics/physics_types.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace kryga
{
namespace physics
{

class destructible_physics;

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

    // Register an independent static triangle-mesh collider (vertices in world
    // space). Additive — does NOT touch the build_static_world body, so multiple
    // colliders (terrain chunks, props) can coexist. Returns an invalid handle
    // on failure (degenerate mesh / uninitialised world). Convenience wrapper for
    // alloc_static_handle() + create_static_mesh().
    static_body_handle
    register_static_mesh(const static_world_mesh& mesh);

    // Split form used by physics_bridge: reserve a collider handle up front (so the
    // model can store it synchronously), then build the Jolt body for that handle
    // later when the register command is drained. create_static_mesh is a no-op on a
    // stale handle or degenerate mesh.
    static_body_handle
    alloc_static_handle();
    void
    create_static_mesh(static_body_handle h, const static_world_mesh& mesh);

    // Remove a collider previously returned by register_static_mesh /
    // alloc_static_handle. No-op on an invalid/unknown handle.
    void
    unregister_static_mesh(static_body_handle h);

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
