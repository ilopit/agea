#pragma once

#include <glm_unofficial/glm.h>

#include <physics/physics_types.h>

#include <utils/laned_pool.h>

#include <cstdint>
#include <memory>
#include <vector>

// Forward-declared so this public header can name the BodyID storage type without
// pulling Jolt in. The laned_storage<…, JPH::BodyID>& return below only needs the
// type spelled, never dereferenced — the full definition lives Jolt-side in impl.
namespace JPH
{
class BodyID;
}

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

    // Static-collider storage. physics_bridge claims lane 0 of this once at init
    // (bind), mirroring how render_translator binds its allocators to the loader's
    // storages. Growth is consumer-side (create_static_mesh on the physics thread),
    // so the allocator never touches it after the claim. Model thread, init-time.
    utils::laned_storage<k_static_collider_kind, JPH::BodyID>&
    static_collider_storage();

    // Build the Jolt body for a bridge-minted collider handle and store it in the
    // slot the handle indexes. Driven by the register command on the physics thread.
    // No-op on a stale handle; a degenerate mesh leaves the slot holding an invalid
    // BodyID (unregister/shutdown treat that as "nothing to destroy").
    void
    create_static_mesh(static_body_handle h, const static_world_mesh& mesh);

    // Destroy the body behind a collider handle and free its slot. Driven by the
    // unregister command on the physics thread. No-op on a stale/empty handle.
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
