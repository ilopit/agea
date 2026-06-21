#pragma once

#include <core/i_processor.h>
#include <core/physics_message.h>
#include <core/physics_result.h>

#include <physics/physics_types.h>

#include <utils/spsc_queue.h>

#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace kryga
{
namespace physics
{
class physics_system;
}

// Consumer-side translator for the physics worker: drains model->physics commands
// onto physics_system calls, advances the simulation on a fixed timestep, and
// publishes physics->model results. The physics analog of render_cmd::dispatch +
// the render loop's draw, and of audio_message_processor + the audio worker's
// renderer.tick — except physics is bidirectional, so this also PRODUCES results.
//
// Drives both the dedicated physics thread (engine_threads_coordinator::physics_loop) and the
// headless inline path (no threads) — the accumulator and active-handle set live
// here so they persist across pumps. Sole consumer of the command ring and sole
// producer of the result ring; sole caller of physics_system once the world is
// handed off (the main thread only init/teardown).
class physics_command_processor : public i_processor
{
public:
    physics_command_processor(physics::physics_system& ps,
                              utils::spsc_queue<core::physics_message>& commands,
                              utils::spsc_queue<core::physics_result>& results)
        : m_ps(ps)
        , m_commands(commands)
        , m_results(results)
    {
    }

    // i_processor::process — one worker iteration: apply queued commands, advance the
    // fixed-step accumulator by `dt` seconds (unless paused), and publish results if it
    // stepped. `frame` is unused (single command ring, no parity). Pause freezes
    // integration (edit mode) but still drains commands so transforms/registrations stay
    // synced for when play resumes — see set_paused().
    void
    process(float dt, uint32_t frame) override;

    // Freeze/resume integration. Set from the main thread (play-mode toggle); read on
    // the physics thread inside process(). Atomic so the cross-thread hand-off is a
    // plain relaxed load each iteration, like the old engine_threads_coordinator flag it replaces.
    void
    set_paused(bool paused)
    {
        m_paused.store(paused, std::memory_order_relaxed);
    }

    // Apply queued commands without stepping or publishing. Used on shutdown so
    // intents queued just before join (e.g. unregister) still reach the world.
    void
    drain_commands();

private:
    // Apply one drained command to physics_system. The physics twin of audio's
    // apply(message) / render's apply(command): the per-message switch, called for each
    // command both drain_commands() and (transitively) process() pull off the ring.
    void
    apply(const core::physics_message& msg);

    bool
    step(float dt);

    void
    publish();

    physics::physics_system& m_ps;
    utils::spsc_queue<core::physics_message>& m_commands;
    utils::spsc_queue<core::physics_result>& m_results;

    // Fixed-step accumulator (seconds) — carries the sub-step remainder between pumps.
    float m_accumulator = 0.0f;

    // Edit-mode freeze. Owned here (physics state), driven by set_paused() from main.
    std::atomic<bool> m_paused{false};

    // Live destructibles -> chunk count, maintained from register/unregister
    // commands. The publish loop walks this to know which handles to report and how
    // many chunk transforms each has.
    std::unordered_map<uint64_t, uint32_t> m_active;
};

}  // namespace kryga
