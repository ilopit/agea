#include "packages/base/model/components/audio_emitter_component.h"

#include <core/audio_message.h>

#include <audio_translator/audio_translator.h>

#include <global_state/global_state.h>

namespace kryga
{
namespace base
{

KRG_gen_class_cd_default(audio_emitter_component);

void
audio_emitter_component::emit_audio_message(const core::audio_message& msg) const
{
    glob::glob_state().getr_audio_translator().emit(msg);
}

bool
audio_emitter_component::construct(construct_params& c)
{
    base_class::construct(c);
    m_tickable = true;
    return true;
}

void
audio_emitter_component::play()
{
    if (!m_clip)
    {
        return;
    }

    core::audio_message msg;
    msg.kind = core::audio_msg_kind::play;
    msg.voice_id = get_id();
    msg.clip_id = m_clip->get_id();
    msg.clip = m_clip;
    msg.volume = m_volume;
    msg.looping = m_loop;
    msg.spatial = m_spatial;
    msg.position = glm::vec3(get_world_position());
    msg.min_distance = m_min_distance;
    msg.max_distance = m_max_distance;
    emit_audio_message(msg);

    m_playing = true;
    // The play message already carries this position; record it so on_tick won't
    // re-send it next frame.
    m_last_emit_pos = msg.position;
}

void
audio_emitter_component::stop()
{
    core::audio_message msg;
    msg.kind = core::audio_msg_kind::stop;
    msg.voice_id = get_id();
    emit_audio_message(msg);

    m_playing = false;
}

bool
audio_emitter_component::is_playing() const
{
    return m_playing;
}

void
audio_emitter_component::begin_play()
{
    if (m_autoplay)
    {
        play();
    }
}

void
audio_emitter_component::end_play()
{
    stop();
}

void
audio_emitter_component::on_tick(float)
{
    // Keep the 3D source glued to the object's world position while it plays —
    // but only stream a new position when it actually moved, so a stationary
    // emitter doesn't post a message every frame.
    if (m_spatial && m_playing)
    {
        glm::vec3 pos = glm::vec3(get_world_position());
        if (pos != m_last_emit_pos)
        {
            m_last_emit_pos = pos;

            core::audio_message msg;
            msg.kind = core::audio_msg_kind::set_position;
            msg.voice_id = get_id();
            msg.position = pos;
            emit_audio_message(msg);
        }
    }
}

}  // namespace base
}  // namespace kryga
