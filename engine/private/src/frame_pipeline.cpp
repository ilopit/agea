#include <engine/frame_pipeline.h>

#include <global_state/global_state.h>

#include <vulkan_render/render_system.h>  // render_system: getr_render() returns this; holds .renderer
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/render_thread.h>  // render-state ownership handoff
#include <render_translator/render_command.h>  // render_exec_context: drain_frame executes commands against it
#include <render_translator/render_translator.h>  // tick_content_allocators()

#include <utils/check.h>

namespace kryga
{

frame_pipeline::~frame_pipeline()
{
    stop();
}

void
frame_pipeline::start()
{
    KRG_check(!m_thread.joinable(), "frame_pipeline::start called twice");
    m_thread = std::thread(&frame_pipeline::render_loop, this);
}

void
frame_pipeline::stop()
{
    if (!m_thread.joinable())
    {
        return;
    }

    // Drain any in-flight frames, then signal shutdown so the render thread
    // breaks out only once it has nothing left to draw.
    {
        std::unique_lock lock(m_mutex);
        m_main_cv.wait(lock, [this] { return m_completed == m_submitted; });
        m_shutdown = true;
    }
    m_render_cv.notify_one();
    m_thread.join();
}

uint32_t
frame_pipeline::begin_frame()
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
    glob::glob_state().getr_render().input_queue.set_build_frame_slot(frame_slot);

    // [model thread] Mature deferred content-slot frees. Once per frame here so the
    // model-owned allocators (which live in render_translator) recycle indices only
    // after the GPU has drained the frames that referenced them.
    glob::glob_state().getr_render_translator().tick_content_allocators();
    return frame_slot;
}

void
frame_pipeline::submit_frame()
{
    {
        std::lock_guard lock(m_mutex);
        ++m_submitted;
    }
    m_render_cv.notify_one();
}

void
frame_pipeline::wait_idle()
{
    std::unique_lock lock(m_mutex);
    m_main_cv.wait(lock, [this] { return m_completed == m_submitted; });
}

bool
frame_pipeline::wait_frames_rendered(int count, std::chrono::milliseconds timeout)
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
frame_pipeline::drain_frame(uint32_t frame_slot)
{
    auto& vr = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;

    render_cmd::render_exec_context exec_ctx{vr, loader};

    // Drain this frame slot's queue to empty. All of the frame's commands were
    // pushed (and made visible via the submitted-counter mutex handoff) before the
    // render thread was released, and the producer is on the other frame slot, so
    // "empty" reliably means "whole frame consumed".
    glob::glob_state()
        .getr_render()
        .input_queue.command_queue(frame_slot)
        .drain(
            [&exec_ctx](render_cmd::render_command_base*&& cmd)
            {
                // Central tagged dispatch: runs the command for its kind, then
                // destructs it (the arena only rewinds, never calls dtors).
                render_cmd::dispatch(cmd, exec_ctx);
            });
}

void
frame_pipeline::render_loop()
{
    // The render thread grants itself render-state access for the lifetime of the
    // loop. Main handed off in vulkan_engine before starting us, so from here only
    // this thread may mutate render state.
    render::set_render_access(true);

    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& queues = glob::glob_state().getr_render().input_queue;

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
        // subsystem); the pipeline only orchestrates when it drains. Render access
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
        drain_frame(frame_slot);
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

}  // namespace kryga
