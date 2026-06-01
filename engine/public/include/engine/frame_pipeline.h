#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace kryga
{

// Coordinates the depth-1 streaming pipeline between the main (producer) thread
// and the render (consumer) thread. Owns the render thread, the two condition
// variables, the submitted/completed frame counters — and the per-frame parity
// slot routing those counters imply.
//
// The slot (frame_id & 1) is one logical value the pipeline is the authority
// for; the renderer and command queues each cache their own copy because the
// slot is ambient context deep in their call trees. begin_frame() routes the
// producer copies (build slot); the render loop stamps the consumer copy (draw
// slot) and drives the frame. Subsystems are reached through glob_state, the
// codebase-wide access pattern — so this owns the whole frame lifecycle without
// the engine acting as a middleman.
//
// Invariant: submitted - completed <= 1. The main thread builds frame N into
// parity slot N&1 while the render thread draws frame N-1 from slot (N-1)&1.
// The vsync / present-pacing stall the render thread takes each frame is exactly
// the window this lets the main thread fill — the point of the decouple.
//
// Per-frame stages:
//   main:   begin_frame()  -> gate until the slot is free, route producer slots
//           ... engine builds the frame into that slot ...
//           submit_frame() -> publish to the render thread, wake it
//   render: (internal) wait for work -> drain + draw + reset slot -> complete
class frame_pipeline
{
public:
    frame_pipeline() = default;
    ~frame_pipeline();

    frame_pipeline(const frame_pipeline&) = delete;
    frame_pipeline&
    operator=(const frame_pipeline&) = delete;

    // Spawn the render thread. Call once before begin_frame/submit_frame.
    void
    start();

    // Drain in-flight frames, signal shutdown, join the render thread.
    // Idempotent: a no-op if never started or already stopped (the dtor calls
    // it, so an explicit stop() is optional).
    void
    stop();

    // --- Main-thread per-frame stages ---

    // Block until the next frame's parity slot is free (depth-1 gate), route the
    // producer slots (renderer build slot + command queue/arena) to it, and
    // return the slot. The engine then builds the frame before submit_frame().
    uint32_t
    begin_frame();

    // Publish the built frame to the render thread and wake it.
    void
    submit_frame();

    // --- Coordination helpers (callable from any thread) ---

    // Block until the render thread has drained everything submitted so far
    // (submitted == completed). For GPU-resource rebuilds that require the
    // render thread fully idle (e.g. render-config apply).
    void
    wait_idle();

    // Block until `count` more frames complete beyond those in flight at the
    // call. Returns false on timeout. Lets an RPC observe a model mutation after
    // it has propagated through a drawn frame into the render cache.
    bool
    wait_frames_rendered(int count, std::chrono::milliseconds timeout);

    // --- Consumer side ---

    // Execute (and destruct) every command in frame-parity `slot`'s queue. Each
    // slot holds exactly one frame's commands (the producer is on the other
    // slot), so draining to empty consumes precisely one frame; the caller then
    // issues the draw. Pulls the renderer/loader/queue from glob_state, holds no
    // pipeline state — hence static, callable by both the streaming render loop
    // (slot = frame parity) and the single-threaded headless tick (slot 0) which
    // bypasses the pipeline entirely.
    //
    // The slot *lifecycle* (set_active_slot / reset_slot / reset_arena) belongs
    // to the queue owner (render::input_queue); this only consumes a slot.
    static void
    drain_frame(uint32_t slot);

private:
    void
    render_loop();

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_render_cv;  // wakes render thread: work available / shutdown
    std::condition_variable m_main_cv;    // wakes producers: a frame completed / pipeline idle
    uint64_t m_submitted = 0;
    uint64_t m_completed = 0;
    bool m_shutdown = false;
};

}  // namespace kryga
