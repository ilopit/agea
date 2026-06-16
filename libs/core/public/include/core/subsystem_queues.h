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
// Capacities are ctor params (default to the render channel's sizing).
//
// alloc_cmd stamps `cmd_kind` only for tagged-command types (those exposing a `static
// constexpr k_kind` — the central-dispatch discriminator). Single-type channels whose
// element has no k_kind simply get allocated, no stamp.
//
// Per-frame-parity queues + arenas. The main thread produces frame F into slot (F&1);
// the render thread consumes the same slot. The engine's depth-1 pipeline gate keeps
// main at most one frame ahead, so producer and consumer never touch the same slot at
// once — each queue carries exactly one frame's commands, and the render thread drains
// its slot to empty (no in-band frame-boundary marker) then draws. Arena reuse is a
// free bump-pointer rewind (reset_frame_slot).
//
// The frame-slot lifecycle (set_build_frame_slot / reset_frame_slot / reset_arena) is
// driven by the frame owner (engine_threads in the streaming loop, the headless tick
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

// Neutral, per-subsystem queue holder — the single model->subsystem boundary producers
// write to INSTEAD of reaching into a subsystem's system object. Lives as its own
// global_state box (not on any system) so no subsystem "owns" the queue. The two
// channels use different mechanisms because their consumers differ:
//   - render: double-buffered command_queue — main builds slot (F&1), the render thread
//             drains the other; pointers into a per-parity arena; sized for a full frame
//             of draw commands.
//   - audio:  lock-free value SPSC ring — audio is fire-and-forget on a dedicated
//             consumer thread (the audio worker in engine_threads), so there's no frame
//             parity and no arena:
//             POD messages are copied straight into the ring. Main is the SOLE producer
//             (emitter intents, listener pose, orphan stops); the audio thread is the
//             SOLE consumer. Sized for a handful of intents per frame.
//
// render_command_base is forward-declared and held only by pointer (type forwarding),
// so core needs no dependency on the render libs; audio_message is a core type. Future
// physics/vfx channels get added here as those subsystems land.
//
// No teardown drop_pending: the render channel self-cleans via its arena rewind on the
// render thread, and the audio channel has exactly one consumer (the audio thread) — a
// main-thread drain would be a second consumer and corrupt the SPSC invariants. Stale
// plays for torn-down emitters are instead cancelled by audio_bridge::reap_orphans,
// which emits stop intents from the model thread.
struct subsystem_queues
{
    command_queue<render_cmd::render_command_base> render;
    utils::spsc_queue<core::audio_message> audio{256};
};

// audio_message crosses the model->audio thread boundary by value and carries only a
// raw model-owned clip pointer (no owned resources). Keep it free of RAII members so a
// copy is a plain field-wise copy with nothing to clean up. (utils::id has a
// user-declared copy ctor, so the message isn't trivially_copyable — destructibility is
// the guard that actually matters here.)
static_assert(std::is_trivially_destructible_v<core::audio_message>,
              "audio_message must stay free of RAII members — it is a plain value "
              "message copied across the model->audio thread boundary");

}  // namespace kryga
