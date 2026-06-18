#pragma once

#include <core/physics_message.h>
#include <core/physics_result.h>

#include <utils/spsc_queue.h>

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
// Drives both the dedicated physics thread (engine_threads::physics_loop) and the
// headless inline path (no threads) — the accumulator and active-handle set live
// here so they persist across pumps. Sole consumer of the command ring and sole
// producer of the result ring; sole caller of physics_system once the world is
// handed off (the main thread only init/teardown).
class physics_command_processor
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

    // One worker iteration: apply queued commands, advance the fixed-step
    // accumulator by `dt` seconds (unless paused), and publish results if it
    // stepped. `paused` freezes integration (edit mode) but still drains commands
    // so transforms/registrations stay synced for when play resumes.
    void
    pump(float dt, bool paused);

    // Apply queued commands without stepping or publishing. Used on shutdown so
    // intents queued just before join (e.g. unregister) still reach the world.
    void
    drain_commands();

private:
    bool
    step(float dt);

    void
    publish();

    physics::physics_system& m_ps;
    utils::spsc_queue<core::physics_message>& m_commands;
    utils::spsc_queue<core::physics_result>& m_results;

    // Fixed-step accumulator (seconds) — carries the sub-step remainder between pumps.
    float m_accumulator = 0.0f;

    // Live destructibles -> chunk count, maintained from register/unregister
    // commands. The publish loop walks this to know which handles to report and how
    // many chunk transforms each has.
    std::unordered_map<uint64_t, uint32_t> m_active;
};

}  // namespace kryga
