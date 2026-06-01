#pragma once

#include <utils/memory_arena.h>
#include <utils/spsc_queue.h>

#include <memory>

namespace kryga
{

namespace render_cmd
{
struct render_command_base;
}

namespace render
{

// The render system's command input — a member of render_system, reached via
// glob_state().getr_render().input_queue (NOT a standalone subsystem, and NOT
// part of core::queues, which holds only model-side dirty tracking). The main
// thread produces render commands into the active parity slot; the render thread
// drains the other slot. From the render system's point of view this is its input.
//
// Per-frame-parity command queues + arenas. Main produces frame F into slot (F&1);
// the render thread consumes the same slot. The engine's depth-1 pipeline gate keeps
// main at most one frame ahead, so the producer and consumer never touch the same
// slot at once — each queue therefore carries exactly one frame's commands, and the
// render thread simply drains its slot to empty (no in-band frame-boundary marker)
// then draws. Arena reuse is a free bump-pointer rewind (reset_slot).
//
// The slot *lifecycle* (set_active_slot / reset_slot / reset_arena) is driven by the
// frame owner (frame_pipeline in the streaming loop, the headless tick otherwise).
class input_queue
{
public:
    template <typename T, typename... Args>
    T*
    alloc_cmd(Args&&... args)
    {
        return m_arenas[m_active_slot].alloc<T>(std::forward<Args>(args)...);
    }

    void*
    alloc_raw(size_t size, size_t align)
    {
        return m_arenas[m_active_slot].alloc_raw(size, align);
    }

    // Enqueue into the active (producer) slot — the frame being built.
    void
    enqueue(render_cmd::render_command_base* cmd)
    {
        m_command_queues[m_active_slot]->push(std::move(cmd));
    }

    // Consumer-side access to a specific slot's queue (render thread).
    utils::spsc_queue<render_cmd::render_command_base*>&
    command_queue(uint32_t slot)
    {
        return *m_command_queues[slot & 1u];
    }

    // Select the active (producer) slot for the frame about to be built:
    // alloc_cmd/alloc_raw/enqueue all target this slot's arena and queue.
    // Does NOT rewind — the render thread's reset_slot does that after it
    // has drawn the frame.
    void
    set_active_slot(uint32_t slot)
    {
        m_active_slot = slot & 1u;
    }

    // Rewind a slot's arena after the render thread has drawn that frame (by
    // then its queue is drained empty and every command destructed). Safe
    // against the main thread, which is building into the *other* slot.
    void
    reset_slot(uint32_t slot)
    {
        m_arenas[slot & 1u].reset();
    }

    // Single-threaded / headless: rewind the active arena after a
    // synchronous drain (commands already executed on the calling thread).
    void
    reset_arena()
    {
        m_arenas[m_active_slot].reset();
    }

private:
    utils::memory_arena m_arenas[2];

    // unique_ptr because spsc_queue is non-movable (atomics) and has an
    // explicit capacity ctor, so it can't be a plain value array element.
    std::unique_ptr<utils::spsc_queue<render_cmd::render_command_base*>> m_command_queues[2] = {
        std::make_unique<utils::spsc_queue<render_cmd::render_command_base*>>(16384),
        std::make_unique<utils::spsc_queue<render_cmd::render_command_base*>>(16384)};
    uint32_t m_active_slot = 0;
};

}  // namespace render
}  // namespace kryga
