#pragma once

#include <core/audio_message.h>

namespace kryga
{

// The model<->audio analog of render_bridge. Model emitters never touch
// audio_system and never touch the command queue directly: they call emit() (the
// producer side), and the bridge translates drained messages into audio_system
// calls via process() (the consumer side). Stateless — it fetches the queue /
// audio_system from global_state at call time (mirrors render_bridge fetching the
// render system). Lives on the main thread, so no arena / double-buffer / threaded
// command machinery is needed (unlike render, whose consumer is a separate thread).
class audio_bridge
{
public:
    // Producer side (model thread): enqueue a model-emitted intent onto the audio
    // channel of subsystem_queues. The only place that writes the audio queue, so the
    // queue's alloc/enqueue mechanics stay out of model components.
    void
    emit(const core::audio_message& msg);

    // Consumer side: translate one drained message into the matching audio_system call.
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
