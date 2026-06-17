#pragma once

namespace kryga
{

namespace physics
{
class physics_system;
}  // namespace physics

namespace physics_cmd
{

// Context handed to a physics command when it is drained, mirroring
// render_cmd::render_exec_context. Physics commands mutate the physics world
// directly (single-threaded, same thread that ticks it).
struct physics_exec_context
{
    physics::physics_system& ps;
};

struct physics_command_base
{
    virtual ~physics_command_base() = default;
    virtual void
    execute(physics_exec_context& ctx) = 0;
};

}  // namespace physics_cmd
}  // namespace kryga
