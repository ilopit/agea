#pragma once

#include <core/audio_message.h>
#include <core/i_processor.h>

#include <utils/spsc_queue.h>

#include <cstdint>

namespace kryga
{
namespace audio
{
class audio_renderer;
}

// Consumer-side translator: drains the audio message ring and maps each
// core::audio_message onto audio_renderer calls. The audio analog of render_cmd::dispatch
// executing a command against the renderer (render_exec_context). It lives in the
// translator layer and is the ONLY place that knows both the model intent format
// (core::audio_message) and the renderer API at once:
//   - audio_translator is producer-only (emit / on_frame) and no longer processes.
//   - audio_renderer knows nothing about audio_message.
//
// Owns the ring + renderer by reference. process() does the whole worker iteration —
// drain the channel and tick the renderer by dt (the tick only reaps finished one-shot
// voices). The audio worker calls it each loop; in headless the main thread calls it
// inline, single-threaded.
class audio_message_processor : public i_processor
{
public:
    audio_message_processor(audio::audio_renderer& renderer,
                            utils::spsc_queue<core::audio_message>& queue)
        : m_renderer(renderer)
        , m_queue(queue)
    {
    }

    // i_processor::process — drain every queued message onto the renderer, then tick it
    // by `dt` seconds (reaps finished voices). `frame` is unused (single ring, no
    // parity). Self-clocked: dt is the wall-clock delta the worker measured (0 in
    // headless, where there is no clock to sample).
    void
    process(float dt, uint32_t frame) override;

private:
    // Apply one drained message to the renderer.
    void
    apply(const core::audio_message& msg);

    audio::audio_renderer& m_renderer;
    utils::spsc_queue<core::audio_message>& m_queue;
};

}  // namespace kryga
