#include <engine/engine_threads.h>

#include <global_state/global_state.h>

#include <vulkan_render/render_system.h>  // render_system: getr_render() returns this; holds .renderer
#include <core/subsystem_queues.h>  // getr_subsystem_queues(): render command channel + audio ring
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/render_thread.h>  // render-state ownership handoff
#include <render_translator/render_command_processor.h>  // render-thread command consumer
#include <render_translator/render_translator.h>  // render_translator::on_frame()

#include <audio/audio_system.h>                    // audio thread owns + ticks the renderer
#include <audio_translator/audio_message_processor.h>  // translates drained messages onto it

#include <physics/physics_system.h>                    // physics thread owns + steps the world
#include <physics_translator/physics_command_processor.h>  // drains commands, steps, publishes results

#include <utils/check.h>

#include <chrono>

namespace kryga
{

engine_threads::~engine_threads()
{
    stop();
}

void
engine_threads::start(audio_message_processor& audio,
                       physics_command_processor& physics,
                       render_command_processor& render)
{
    KRG_check(!m_render_thread.joinable(), "engine_threads::start called twice");

    // Borrow the engine-owned processors the audio / physics / render loops drive.
    m_audio_processor = &audio;
    m_physics_processor = &physics;
    m_render_processor = &render;

    // Render thread — the gated streaming pipeline.
    m_shutdown = false;
    m_render_thread = std::thread(&engine_threads::render_loop, this);

    // Audio thread — independent fire-and-forget worker.
    m_audio_shutdown.store(false, std::memory_order_relaxed);
    m_audio_thread = std::thread(&engine_threads::audio_loop, this);

    // Physics thread — independent self-clocked worker (bidirectional: drains command
    // intents, steps Jolt on a fixed timestep, publishes result snapshots).
    m_physics_shutdown.store(false, std::memory_order_relaxed);
    m_physics_thread = std::thread(&engine_threads::physics_loop, this);
}

void
engine_threads::stop()
{
    // Render thread first: drain any in-flight frames, then signal shutdown so it
    // breaks out only once it has nothing left to draw.
    if (m_render_thread.joinable())
    {
        {
            std::unique_lock lock(m_mutex);
            m_main_cv.wait(lock, [this] { return m_completed == m_submitted; });
            m_shutdown = true;
        }
        m_render_cv.notify_one();
        m_render_thread.join();
    }

    // Audio thread: flag + join. Its loop does a final drain on the way out so
    // intents queued just before shutdown (e.g. end_play stops) still apply.
    if (m_audio_thread.joinable())
    {
        m_audio_shutdown.store(true, std::memory_order_release);
        m_audio_thread.join();
    }

    // Physics thread: flag + join. Like audio, it does a final command drain on the
    // way out so intents queued just before shutdown (e.g. unregister) still apply
    // before physics_system is torn down. Joining here is the barrier that makes the
    // subsequent main-thread physics_system::shutdown() safe.
    if (m_physics_thread.joinable())
    {
        m_physics_shutdown.store(true, std::memory_order_release);
        m_physics_thread.join();
    }
}

void
engine_threads::set_physics_paused(bool paused)
{
    m_physics_paused.store(paused, std::memory_order_relaxed);
}

uint32_t
engine_threads::begin_frame()
{
    uint32_t frame_slot;
    {
        std::unique_lock lock(m_mutex);
        // Depth-1 gate: don't reuse a frame slot the render thread hasn't freed.
        // The main thread is the sole writer of m_submitted, so the frame slot
        // computed here stays valid until submit_frame bumps it.
        m_main_cv.wait(lock, [this] { return m_submitted - m_completed <= 1; });
        frame_slot = static_cast<uint32_t>(m_submitted & 1ull);
    }

    // Route the build (producer) frame slot outside the lock — both subsystems name
    // the same parity, set together so they can't drift: the renderer (camera + UI
    // snapshot double buffers) and the command queue/arena. The render thread reads
    // the matching frame slot when it draws this frame.
    glob::glob_state().getr_render().renderer.set_build_frame_slot(frame_slot);
    glob::glob_state().getr_subsystem_queues().render.set_build_frame_slot(frame_slot);

    // [model thread] Mature deferred content-slot frees. Once per frame here so the
    // model-owned allocators (which live in render_translator) recycle indices only
    // after the GPU has drained the frames that referenced them.
    glob::glob_state().getr_render_translator().on_frame();
    return frame_slot;
}

void
engine_threads::submit_frame()
{
    {
        std::lock_guard lock(m_mutex);
        ++m_submitted;
    }
    m_render_cv.notify_one();
}

void
engine_threads::wait_idle()
{
    std::unique_lock lock(m_mutex);
    m_main_cv.wait(lock, [this] { return m_completed == m_submitted; });
}

bool
engine_threads::wait_frames_rendered(int count, std::chrono::milliseconds timeout)
{
    // count < 1 is a caller bug: the static_cast<uint64_t> below would wrap a
    // negative into a huge target and silently block for the full timeout.
    KRG_check(count >= 1, "wait_frames_rendered: count must be >= 1");

    std::unique_lock lock(m_mutex);
    // Frames in flight now; wait until the render thread completes `count` beyond
    // them. The mutation we want to observe rides the frame currently building
    // (or one already submitted), so a completed count past the current
    // submission point guarantees its commands have drained into the cache.
    const uint64_t target = m_submitted + static_cast<uint64_t>(count);
    return m_main_cv.wait_for(lock, timeout, [this, target] { return m_completed >= target; });
}

void
engine_threads::render_loop()
{
    // The render thread grants itself render-state access for the lifetime of the
    // loop. Main handed off in vulkan_engine before starting us, so from here only
    // this thread may mutate render state.
    render::set_render_access(true);

    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& queues = glob::glob_state().getr_subsystem_queues().render;

    // Same handoff for the pool thread guards: init minted system resources and
    // populated storages on the main thread; from here the render thread is the
    // sole system-allocator minter AND the sole storage consumer (populate /
    // grow_for / reset at command drain).
    renderer.bind_render_pools_to_current_thread();

    for (;;)
    {
        // Wait for a submitted-but-undrawn frame or shutdown.
        bool shutting_down = false;
        {
            std::unique_lock lock(m_mutex);
            m_render_cv.wait(lock, [this] { return m_submitted > m_completed || m_shutdown; });
            shutting_down = m_shutdown && m_submitted == m_completed;
        }

        // Render-thread tasks (RPC introspection, editor resource builds) run at
        // the top of each turn — the mirror of drain_main_actions() at the top of
        // vulkan_engine::tick. The queue lives on the renderer (render-thread
        // subsystem); this only orchestrates when it drains. Render access
        // held, no concurrent mutator, so tasks read the cache without a lock and
        // see the last fully drawn frame's state. Drained on the shutdown wake too,
        // so pending tasks complete.
        renderer.drain_render_actions();

        if (shutting_down)
        {
            break;
        }

        // This frame used frame slot (completed & 1). Execute its build/destroy/
        // transform commands, then draw — the frame slot drives the camera/UI
        // snapshot reads inside draw_main, keeping them in lock-step with the frame
        // the main thread produced.
        const uint32_t frame_slot = static_cast<uint32_t>(m_completed & 1ull);
        m_render_processor->drain(frame_slot);
        renderer.set_draw_frame_slot(frame_slot);
        renderer.draw_main();

        // Frame drawn — its queue is drained empty and every command destructed, so
        // rewind the arena for reuse. Safe: the main thread is building into the
        // other frame slot, and the pipeline gate won't let it touch this one until
        // the completion below is published.
        queues.reset_frame_slot(frame_slot);

        {
            std::lock_guard lock(m_mutex);
            ++m_completed;
        }
        // notify_all, not notify_one: the main thread's pipeline gate plus any
        // wait_idle / wait_frames_rendered callers (e.g. RPC threads) are all
        // parked on m_main_cv.
        m_main_cv.notify_all();
    }
}

void
engine_threads::audio_loop()
{
    // Fire-and-forget control worker. Sole consumer of the audio channel and sole
    // owner of audio_renderer from here; main only produces messages. Shares nothing
    // with the render pipeline above — no gate, no frame parity.
    auto& q = glob::glob_state().getr_subsystem_queues().audio;
    auto* as = glob::glob_state().get_audio_system();
    KRG_check(as, "engine_threads::audio_loop: audio_system not created before start()");
    KRG_check(m_audio_processor, "engine_threads::audio_loop: audio processor not bound");
    audio::audio_renderer& renderer = as->renderer;
    audio_message_processor& proc = *m_audio_processor;

    // Self-clocked tick: audio_renderer::tick only reaps finished one-shot voices, so a
    // coarse, locally measured dt is fine — nothing downstream needs it to match the
    // render frame time.
    auto last = std::chrono::steady_clock::now();

    while (!m_audio_shutdown.load(std::memory_order_acquire))
    {
        q.drain([&proc](core::audio_message msg) { proc.process(msg); });

        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        renderer.tick(dt);

        // Poll cadence — miniaudio mixes on its own realtime thread, so this only
        // bounds how quickly a play/stop intent is registered, not audio quality.
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Final drain so intents queued just before shutdown (e.g. end_play stops) are
    // applied before audio_renderer is torn down.
    q.drain([&proc](core::audio_message msg) { proc.process(msg); });
}

void
engine_threads::physics_loop()
{
    // Self-clocked simulation worker. Sole consumer of the physics command ring and
    // sole producer of the result ring; sole owner of the Jolt world from here (main
    // init'd it before start()). Independent of the render pipeline — no gate, no
    // frame parity. Bidirectional: unlike audio it publishes results back.
    KRG_check(m_physics_processor, "engine_threads::physics_loop: physics processor not bound");
    physics_command_processor& proc = *m_physics_processor;

    // Self-clocked tick: the processor's fixed-step accumulator turns this locally
    // measured wall-clock dt into a deterministic number of 1/60s sub-steps, so the
    // coarse poll cadence below doesn't affect simulation stability.
    auto last = std::chrono::steady_clock::now();

    while (!m_physics_shutdown.load(std::memory_order_acquire))
    {
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        proc.pump(dt, m_physics_paused.load(std::memory_order_relaxed));

        // Poll cadence — finer than audio's 5ms so accumulated dt lands close to the
        // fixed-step boundary, bounding how late a fresh command/step is applied.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Final command drain so intents queued just before shutdown (e.g. unregister)
    // reach the world before physics_system is torn down. No step/publish — the model
    // is gone, results would never be read.
    proc.drain_commands();
}

}  // namespace kryga
