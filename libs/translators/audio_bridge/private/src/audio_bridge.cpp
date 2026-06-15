#include "audio_bridge/audio_bridge.h"

#include <audio/audio_system.h>

#include <core/model_system.h>
#include <core/subsystem_queues.h>

#include <global_state/global_state.h>

#include <packages/root/model/assets/audio_clip.h>

#include <vector>

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
    // Single-thread channel (slot 0): copy the intent into the arena and enqueue. The
    // consumer (engine drain) pops these the same frame and feeds them to process().
    auto& q = glob::glob_state().getr_subsystem_queues().audio;
    q.enqueue(q.alloc_cmd<core::audio_message>(msg));
}

void
audio_bridge::process(const core::audio_message& msg)
{
    auto* as = glob::glob_state().get_audio_system();
    if (!as)
    {
        return;
    }

    switch (msg.kind)
    {
    case core::audio_msg_kind::play:
    {
        // The clip is a model-owned pointer carried by the message, valid this
        // frame; load_clip decodes its PCM (idempotent if already loaded).
        if (msg.clip)
        {
            as->load_clip(*msg.clip);
        }
        audio::play_params p;
        p.volume = msg.volume;
        p.looping = msg.looping;
        p.spatial = msg.spatial;
        p.position = msg.position;
        p.min_distance = msg.min_distance;
        p.max_distance = msg.max_distance;
        as->play(msg.voice_id, msg.clip_id, p);
        break;
    }
    case core::audio_msg_kind::stop:
        as->stop(msg.voice_id);
        break;
    case core::audio_msg_kind::set_position:
        as->set_voice_position(msg.voice_id, msg.position);
        break;
    case core::audio_msg_kind::set_volume:
        as->set_voice_volume(msg.voice_id, msg.volume);
        break;
    }
}

void
audio_bridge::reap_orphans()
{
    auto* as = glob::glob_state().get_audio_system();
    if (!as)
    {
        return;
    }

    std::vector<utils::id> ids;
    as->collect_active_voice_ids(ids);
    if (ids.empty())
    {
        return;
    }

    // A voice id is its emitter's id. If the emitter is no longer in the model
    // object cache it was deleted / unloaded / rolled back — stop the voice.
    auto& caches = glob::glob_state().getr_model().caches;
    for (const auto& id : ids)
    {
        if (caches.objects.get_item(id) == nullptr)
        {
            as->stop(id);
        }
    }
}

}  // namespace kryga
