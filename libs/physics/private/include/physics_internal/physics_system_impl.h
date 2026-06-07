#pragma once

// Private header — defines physics_system::impl so both physics_system.cpp
// and destructible_physics.cpp (which is a friend of physics_system) can
// access Jolt-level state. NOT part of the public API.

#include <physics/physics_system.h>

#include "physics_internal/jolt_layers.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <memory>
#include <unordered_map>

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

    // Independently-registered static colliders (terrain, etc.), keyed by the
    // value of the static_body_handle handed back to the caller.
    std::unordered_map<uint64_t, JPH::BodyID> static_bodies;
    uint64_t next_static_body_id = 1;
};

}  // namespace physics
}  // namespace kryga
