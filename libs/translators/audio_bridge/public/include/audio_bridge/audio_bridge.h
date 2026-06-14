#pragma once

#include <core/audio_message.h>

namespace kryga
{

// The model<->audio analog of render_bridge: model emitters never touch
// audio_system, they emit core::audio_message on the model queue, and this bridge
// translates them into audio_system calls. Stateless — it fetches audio_system
// from global_state at call time (mirrors render_bridge fetching the render
// system). Lives on the main thread, so no arena / double-buffer / threaded
// command machinery is needed (unlike render, whose consumer is a separate thread).
class audio_bridge
{
public:
    // Translate one model message into the matching audio_system call.
    void
    process(const core::audio_message& msg);

    // Stop any live voice whose owning emitter is gone from the model cache.
    // Covers object deletion, level unload, and rollback of play-spawned emitters
    // in a single place — it keys off cache membership, so it can't miss a
    // teardown path. Call once per frame after draining the message queue.
    void
    reap_orphans();
};

}  // namespace kryga
