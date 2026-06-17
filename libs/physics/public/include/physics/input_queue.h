#pragma once

#include <utils/memory_arena.h>

#include <cstddef>
#include <utility>
#include <vector>

namespace kryga
{

namespace physics_cmd
{
struct physics_command_base;
}

namespace physics
{

// The physics system's command input — a member of physics_system, reached via
// glob_state().getr_physics_system().input_queue. The physics_bridge produces
// commands here; they are drained synchronously just before physics_system::tick().
//
// Unlike render::input_queue this is NOT double-buffered: physics is ticked on the
// main thread right after the model update, so the producer (physics_bridge during
// the model→physics pass) and the consumer (the pre-tick drain) are the same thread
// and never overlap. A single arena + queue therefore suffices; the arena is a free
// bump-pointer rewind after each drain.
class input_queue
{
public:
    template <typename T, typename... Args>
    T*
    alloc_cmd(Args&&... args)
    {
        return m_arena.alloc<T>(std::forward<Args>(args)...);
    }

    void*
    alloc_raw(size_t size, size_t align)
    {
        return m_arena.alloc_raw(size, align);
    }

    void
    enqueue(physics_cmd::physics_command_base* cmd)
    {
        m_commands.push_back(cmd);
    }

    // Run fn over every queued command (fn executes + destructs it), then clear the
    // queue and rewind the arena. fn carries the full command type so this header
    // need not see physics_command_base's definition (mirrors render's drain path,
    // which lives on the engine consumer side).
    template <typename Fn>
    void
    drain(Fn&& fn)
    {
        for (auto* cmd : m_commands)
        {
            fn(cmd);
        }
        m_commands.clear();
        m_arena.reset();
    }

private:
    utils::memory_arena m_arena;
    std::vector<physics_cmd::physics_command_base*> m_commands;
};

}  // namespace physics
}  // namespace kryga
