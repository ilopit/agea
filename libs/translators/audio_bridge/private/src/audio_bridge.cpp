#include "audio_bridge/audio_bridge.h"

#include <core/model_system.h>
#include <core/subsystem_queues.h>

#include <global_state/global_state.h>

namespace kryga
{

void
state_mutator__audio_bridge::set(gs::state& s)
{
    auto p = s.create_box<audio_bridge>("audio_bridge");
    s.m_audio_bridge = p;
}

void
audio_bridge::emit(const core::audio_message& msg)
{
    // Maintain the main-thread voice mirror so reap_orphans can find orphaned voices
    // without reaching into audio_system (which the audio thread owns).
    switch (msg.kind)
    {
    case core::audio_msg_kind::play:
        m_started_voices.insert(msg.voice_id);
        break;
    case core::audio_msg_kind::stop:
        m_started_voices.erase(msg.voice_id);
        break;
    default:
        break;
    }

    // Value SPSC: copy the POD intent into the ring; the audio thread pops it and
    // feeds it to process(). push() spin-blocks if full, but the queue is sized well
    // above the realistic per-frame intent count.
    glob::glob_state().getr_subsystem_queues().audio.push(core::audio_message(msg));
}

void
audio_bridge::reap_orphans()
{
    if (m_started_voices.empty())
    {
        return;
    }

    // A voice id is its emitter's id. If the emitter is no longer in the model object
    // cache it was deleted / unloaded / rolled back — emit a stop and forget the voice.
    // Model-thread only: reads the cache and the local mirror, produces stop intents;
    // the audio thread applies them through process(). No audio_system access here.
    auto& caches = glob::glob_state().getr_model().caches;
    auto& q = glob::glob_state().getr_subsystem_queues().audio;
    for (auto it = m_started_voices.begin(); it != m_started_voices.end();)
    {
        if (caches.objects.get_item(*it) == nullptr)
        {
            core::audio_message msg;
            msg.kind = core::audio_msg_kind::stop;
            msg.voice_id = *it;
            q.push(core::audio_message(msg));
            it = m_started_voices.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

}  // namespace kryga
