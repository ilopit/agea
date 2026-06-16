#pragma once

#include <core/audio_message.h>

namespace kryga
{
namespace audio
{
class audio_renderer;
}

// Consumer-side translator: maps one drained core::audio_message onto audio_renderer
// calls. The audio analog of render_cmd::dispatch executing a command against the
// renderer (render_exec_context). It lives in the translator layer and is the ONLY
// place that knows both the model intent format (core::audio_message) and the
// renderer API at once:
//   - audio_bridge is producer-only (emit / reap_orphans) and no longer processes.
//   - audio_renderer knows nothing about audio_message.
//
// Holds the renderer by reference; the audio worker constructs one with the
// audio_system's renderer and drains the channel through it. (In headless the main
// thread does the same single-threaded.)
class audio_message_processor
{
public:
    explicit audio_message_processor(audio::audio_renderer& renderer)
        : m_renderer(renderer)
    {
    }

    void
    process(const core::audio_message& msg);

private:
    audio::audio_renderer& m_renderer;
};

}  // namespace kryga
