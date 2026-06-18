#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace kryga
{

// The single owner of every engine worker thread — start()/stop() here is the one
// authority for thread lifecycle and (load-bearing) shutdown ordering. It runs two
// INDEPENDENT threads that share nothing:
//
//   1. Render thread — the depth-1 streaming pipeline between the main (producer)
//      thread and the render (consumer) thread. Owns the two condition variables,
//      the submitted/completed frame counters, and the per-frame "frame slot"
//      routing those counters imply.
//
//   2. Audio thread — a fire-and-forget control worker. NOT on the render thread:
//      audio decode (load_clip) must never stall the frame-critical path. It is the
//      sole owner of audio_renderer and the sole consumer of subsystem_queues().audio
//      (a lock-free value SPSC ring); the main thread only ever produces messages.
//      No frame parity, no gate — its own loop, its own shutdown flag, completely
//      separate from the render machinery below. miniaudio already mixes on its own
//      realtime thread, so this worker just decodes clips, applies model intents, and
//      reaps finished voices off the main thread's hot path.
//
// --- Render pipeline (thread 1) ---
//
// A frame slot (frame_id & 1) is the frame-parity index into the depth-1 double
// buffers; it's one logical value the pipeline is the authority for. The renderer
// and command queue each cache their own copy because the frame slot is ambient
// context deep in their call trees. begin_frame() routes the producer copies (the
// build frame slot); the render loop stamps the consumer copy (the draw frame
// slot) and drives the frame. Subsystems are reached through glob_state, the
// codebase-wide access pattern.
//
// Invariant: submitted - completed <= 1. The main thread builds frame N into
// frame slot N&1 while the render thread draws frame N-1 from frame slot (N-1)&1.
// The vsync / present-pacing stall the render thread takes each frame is exactly
// the window this lets the main thread fill — the point of the decouple.
//
// Per-frame stages:
//   main:   begin_frame()  -> gate until the frame slot is free, route producers
//           ... engine builds the frame into that frame slot ...
//           submit_frame() -> publish to the render thread, wake it
//   render: (internal) wait for work -> drain + draw + reset frame slot -> complete
class engine_threads
{
public:
    engine_threads() = default;
    ~engine_threads();

    engine_threads(const engine_threads&) = delete;
    engine_threads&
    operator=(const engine_threads&) = delete;

    // Spawn the render thread, the audio thread, AND the physics thread. Call once
    // before begin_frame/submit_frame. (Render-state access must already have been
    // handed off to the render thread by the caller — see render::set_render_access;
    // likewise the physics world must be init'd on the main thread first, after which
    // only the physics thread touches it.)
    void
    start();

    // Drain in-flight frames, signal shutdown, and join ALL THREE threads. Idempotent:
    // a no-op if never started or already stopped (the dtor calls it, so an explicit
    // stop() is optional). Must run before audio_system / render / physics subsystems
    // are destroyed — every worker thread touches them.
    void
    stop();

    // Freeze/resume physics integration (edit vs play mode). When paused the physics
    // worker still drains its command queue — registrations and transform syncs keep
    // flowing — but does not advance the simulation. Callable from the main thread.
    void
    set_physics_paused(bool paused);

    // --- Main-thread per-frame stages (render pipeline) ---

    // Block until the next frame slot is free (depth-1 gate), route the producers
    // (renderer build frame slot + command queue/arena) to it, and return the frame
    // slot. The engine then builds the frame before submit_frame().
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

    // --- Consumer side (render pipeline) ---

    // Execute (and destruct) every command in `frame_slot`'s queue. Each frame
    // slot holds exactly one frame's commands (the producer is on the other frame
    // slot), so draining to empty consumes precisely one frame; the caller then
    // issues the draw. Pulls the renderer/loader/queue from glob_state, holds no
    // pipeline state — hence static, callable by both the streaming render loop
    // (frame_slot = frame parity) and the single-threaded headless tick (frame slot
    // 0) which bypasses the pipeline entirely.
    //
    // The frame-slot lifecycle (set_build_frame_slot / reset_frame_slot /
    // reset_arena) belongs to the queue owner (getr_subsystem_queues().render); this only
    // consumes a frame slot.
    static void
    drain_frame(uint32_t frame_slot);

private:
    void
    render_loop();

    void
    audio_loop();

    void
    physics_loop();

    // --- Render pipeline (thread 1) ---
    std::thread m_render_thread;
    std::mutex m_mutex;
    std::condition_variable m_render_cv;  // wakes render thread: work available / shutdown
    std::condition_variable m_main_cv;    // wakes producers: a frame completed / pipeline idle
    uint64_t m_submitted = 0;
    uint64_t m_completed = 0;
    bool m_shutdown = false;

    // --- Audio worker (thread 2) — independent of everything above ---
    std::thread m_audio_thread;
    std::atomic<bool> m_audio_shutdown{false};

    // --- Physics worker (thread 3) — independent like audio, but bidirectional ---
    // Self-clocked fixed-step worker. Sole consumer of subsystem_queues().physics.in and
    // sole producer of subsystem_queues().physics.out; the main thread only
    // produces commands / drains results. m_physics_paused freezes integration in edit
    // mode without stopping command drain.
    std::thread m_physics_thread;
    std::atomic<bool> m_physics_shutdown{false};
    std::atomic<bool> m_physics_paused{false};
};

}  // namespace kryga
