#pragma once

// Private header — defines physics_system::impl so both physics_system.cpp
// and destructible_physics.cpp (which is a friend of physics_system) can
// access Jolt-level state. NOT part of the public API.

#include <physics/physics_system.h>

#include "physics_internal/jolt_layers.h"

#include <utils/laned_pool.h>

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <memory>

namespace kryga
{
namespace physics
{

struct physics_system::impl
{
    std::unique_ptr<JPH::TempAllocator> temp_allocator;
    std::unique_ptr<JPH::JobSystem> job_system;
    std::unique_ptr<JPH::PhysicsSystem> world;

    jolt_layers::bp_layer_interface_impl bp_layers;
    jolt_layers::object_vs_bp_filter_impl obj_vs_bp;
    jolt_layers::object_layer_pair_filter_impl obj_vs_obj;

    JPH::BodyID static_world_body;  // invalid == none

    // Static-collider BodyIDs, indexed by the bridge-minted handle (kind
    // k_static_collider_kind). The render split applied to physics: physics_translator
    // owns the lane_allocator (mints handles on the model thread, claims lane 0);
    // physics_system owns this single-lane laned_storage and is the CONSUMER —
    // grow_for + populate on register, read + reset on unregister, all on the
    // physics thread (growth is consumer-side now, so no cross-thread grow race).
    // A slot holds an invalid BodyID between register and a successful
    // create_static_mesh (and for a degenerate mesh). The shutdown sweep runs on
    // main AFTER the physics thread is joined; it re-stamps the lane affinity to
    // main (bind_to_current_thread) before reading.
    utils::laned_storage<k_static_collider_kind, JPH::BodyID> static_storage{1};
};

}  // namespace physics
}  // namespace kryga
