#pragma once

#include <core/audio_message.h>

#include <utils/memory_arena.h>
#include <utils/spsc_queue.h>

#include <memory>
#include <type_traits>

namespace kryga
{

namespace render_cmd
{
struct render_command_base;
}

// Generic, double-buffered command channel (NOT render-specific — hence no vulkan
// dependency). TCmd is the command base type whose pointers flow through the queue.
// Capacities are ctor params (default to the render channel's sizing); a single-thread
// channel like audio can request small arenas/queues.
//
// alloc_cmd stamps `cmd_kind` only for tagged-command types (those exposing a `static
// constexpr k_kind` — the central-dispatch discriminator). Single-type channels whose
// element has no k_kind (e.g. audio_message) simply get allocated, no stamp.
//
// Per-frame-parity queues + arenas. The main thread produces frame F into slot (F&1);
// the render thread consumes the same slot. The engine's depth-1 pipeline gate keeps
// main at most one frame ahead, so producer and consumer never touch the same slot at
// once — each queue carries exactly one frame's commands, and the render thread drains
// its slot to empty (no in-band frame-boundary marker) then draws. Arena reuse is a
// free bump-pointer rewind (reset_frame_slot). A single-thread channel ignores the
// parity and just uses slot 0 (build, drain, reset_arena — all same thread, same frame).
//
// The frame-slot lifecycle (set_build_frame_slot / reset_frame_slot / reset_arena) is
// driven by the frame owner (frame_pipeline in the streaming loop, the headless tick
// otherwise). A "frame slot" is the frame-parity index (frame & 1) selecting one of the
// two depth-1 double buffers.
template <typename TCmd>
class command_queue
{
public:
    explicit command_queue(size_t queue_capacity = 16384, size_t arena_capacity = 4 * 1024 * 1024)
        : m_arenas{utils::memory_arena(arena_capacity), utils::memory_arena(arena_capacity)}
        , m_command_queues{std::make_unique<utils::spsc_queue<TCmd*>>(queue_capacity),
                           std::make_unique<utils::spsc_queue<TCmd*>>(queue_capacity)}
    {
    }

    template <typename T, typename... Args>
    T*
    alloc_cmd(Args&&... args)
    {
        T* p = m_arenas[m_build_frame_slot].alloc<T>(std::forward<Args>(args)...);
        if constexpr (requires { T::k_kind; })
        {
            p->cmd_kind = T::k_kind;
        }
        return p;
    }

    void*
    alloc_raw(size_t size, size_t align)
    {
        return m_arenas[m_build_frame_slot].alloc_raw(size, align);
    }

    // Enqueue into the build (producer) frame slot — the frame being built.
    void
    enqueue(TCmd* cmd)
    {
        m_command_queues[m_build_frame_slot]->push(std::move(cmd));
    }

    // Consumer-side access to a specific frame slot's queue (render thread).
    utils::spsc_queue<TCmd*>&
    queue(uint32_t frame_slot)
    {
        return *m_command_queues[frame_slot & 1u];
    }

    // Select the build (producer) frame slot for the frame about to be built:
    // alloc_cmd/alloc_raw/enqueue all target this slot's arena and queue.
    // Does NOT rewind — the render thread's reset_frame_slot does that after it
    // has drawn the frame.
    void
    set_build_frame_slot(uint32_t frame_slot)
    {
        m_build_frame_slot = frame_slot & 1u;
    }

    // Rewind a frame slot's arena after the render thread has drawn that frame (by
    // then its queue is drained empty and every command destructed). Safe against
    // the main thread, which is building into the *other* frame slot.
    void
    reset_frame_slot(uint32_t frame_slot)
    {
        m_arenas[frame_slot & 1u].reset();
    }

    // Single-threaded / headless: rewind the build arena after a synchronous
    // drain (commands already executed on the calling thread).
    void
    reset_arena()
    {
        m_arenas[m_build_frame_slot].reset();
    }

private:
    utils::memory_arena m_arenas[2];

    // unique_ptr because spsc_queue is non-movable (atomics) and has an
    // explicit capacity ctor, so it can't be a plain value array element.
    std::unique_ptr<utils::spsc_queue<TCmd*>> m_command_queues[2];
    uint32_t m_build_frame_slot = 0;
};

// Neutral, per-subsystem command-queue holder — the single model->subsystem boundary
// producers write to INSTEAD of reaching into a subsystem's system object. Lives as its
// own global_state box (not on any system) so no subsystem "owns" the queue. Both
// channels are the same command_queue mechanism; they differ only in usage:
//   - render: cross-thread, double-buffered — main builds slot (F&1), render thread
//             drains the other; sized for a full frame of draw commands.
//   - audio:  single-thread — main builds and drains slot 0 the same frame, so the
//             parity/double-buffer is unused; sized small (a handful of intents/frame).
//
// render_command_base is forward-declared and held only by pointer (type forwarding),
// so core needs no dependency on the render libs; audio_message is a core type. Future
// physics/vfx channels get added here as those subsystems land.
struct subsystem_queues
{
    command_queue<render_cmd::render_command_base> render;
    command_queue<core::audio_message> audio{256, 64 * 1024};

    // Drop pending intents on level teardown / play-mode rollback. Only audio: the
    // render channel is drained by the render thread and self-cleans via its arena
    // rewind; touching it here would race that thread. Audio is single-thread (slot 0),
    // so discarding the queue + rewinding the arena on the main thread is safe.
    void
    drop_pending()
    {
        audio.queue(0).drain([](core::audio_message*) {});
        audio.reset_arena();
    }
};

// audio drain + arena reset never run destructors (arena reset is a bump-pointer
// rewind), so audio_message must stay trivially destructible.
static_assert(std::is_trivially_destructible_v<core::audio_message>,
              "audio_message must be trivially destructible: the audio command_queue's "
              "arena reset does not run destructors");

}  // namespace kryga
