#include "audio_translator/audio_message_processor.h"

#include <audio/audio_renderer.h>

#include <packages/root/model/assets/audio_clip.h>

namespace kryga
{

void
audio_message_processor::process(float dt, uint32_t /*frame*/)
{
    // Drain the whole channel onto the renderer, then advance it. Mirrors physics's
    // process (drain commands -> step): one self-contained worker iteration.
    m_queue.drain([this](core::audio_message msg) { apply(msg); });
    m_renderer.tick(dt);
}

void
audio_message_processor::apply(const core::audio_message& msg)
{
    switch (msg.kind)
    {
    case core::audio_msg_kind::play:
    {
        // The clip is a model-owned pointer carried by the message (see the lifetime
        // note in audio_message.h); load_clip decodes its PCM, idempotent if loaded.
        if (msg.clip)
        {
            m_renderer.load_clip(*msg.clip);
        }
        audio::play_params p;
        p.volume = msg.volume;
        p.looping = msg.looping;
        p.spatial = msg.spatial;
        p.position = msg.position;
        p.min_distance = msg.min_distance;
        p.max_distance = msg.max_distance;
        m_renderer.play(msg.voice_id, msg.clip_id, p);
        break;
    }
    case core::audio_msg_kind::stop:
        m_renderer.stop(msg.voice_id);
        break;
    case core::audio_msg_kind::set_position:
        m_renderer.set_voice_position(msg.voice_id, msg.position);
        break;
    case core::audio_msg_kind::set_volume:
        m_renderer.set_voice_volume(msg.voice_id, msg.volume);
        break;
    case core::audio_msg_kind::set_listener:
        m_renderer.set_listener(msg.position, msg.forward, msg.up);
        break;
    }
}

}  // namespace kryga
