#pragma once

#include <core/audio_message.h>
#include <core/translator_base.h>

#include <utils/id.h>

#include <unordered_set>

namespace kryga
{

// The model<->audio analog of render_translator: PRODUCER-ONLY. Model emitters never
// touch audio_renderer and never touch the queue directly — they call emit(), and the
// bridge pushes the intent onto the audio channel. Translating drained messages back
// into renderer calls is NOT here; it lives in audio_message_processor (the consumer
// side), mirroring how render_translator builds commands while render_cmd::dispatch
// executes them.
//
// Threading: both emit() and on_frame() run on the model/main thread. The only
// state is m_started_voices, touched solely by those two methods.
class audio_translator : public translator_base<core::audio_message>
{
public:
    // Binds the producer base to the audio channel (queues.audio). Model thread.
    audio_translator();

    // Producer side (model thread): maintain the voice mirror, then push a model-emitted
    // intent onto the audio channel via translator_base::emit. The only place that writes
    // the audio queue, so the queue mechanics stay out of model components.
    void
    emit(const core::audio_message& msg);

    // i_translator::on_frame — stop any voice whose owning emitter has left the model
    // cache (object deletion, level unload, play->edit rollback). Pure model-thread
    // work: it diffs the local voice mirror against the model cache and EMITS stop
    // intents — it never touches audio_renderer (the audio thread owns it). Once per frame.
    void
    on_frame() override;

private:
    // Main-thread mirror of the voices we've started, keyed by emitter/voice id.
    // Lets on_frame() detect an orphaned voice without querying audio_renderer
    // across the thread boundary — both this set and the model cache are
    // main-thread, so the diff is race-free. Maintained only by emit() (insert on
    // play / erase on stop) and on_frame().
    std::unordered_set<utils::id> m_started_voices;
};

}  // namespace kryga
