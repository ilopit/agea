#pragma once

#include "physics_bridge/physics_command.h"

#include <physics/physics_types.h>

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <vector>

namespace kryga
{

// Build the Jolt body for a collider handle reserved at build time. The vertices
// are already in world space (the builder flattens them through the component
// transform). The arena-allocated command carries the vectors by value; the
// drain calls the virtual destructor, so the heap buffers are freed there.
struct register_static_collider_cmd : physics_cmd::physics_command_base
{
    physics::static_body_handle handle;
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;

    void
    execute(physics_cmd::physics_exec_context& ctx) override;
};

// Remove a previously registered static collider and free its handle slot.
struct unregister_static_collider_cmd : physics_cmd::physics_command_base
{
    physics::static_body_handle handle;

    void
    execute(physics_cmd::physics_exec_context& ctx) override;
};

}  // namespace kryga
