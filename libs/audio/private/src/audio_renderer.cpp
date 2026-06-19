#include "audio/audio_renderer.h"

#include <packages/root/model/assets/audio_clip.h>

#include <utils/kryga_log.h>

#include <miniaudio.h>

#include <unordered_map>
#include <vector>

namespace kryga
{
namespace audio
{

// Decoded PCM for one clip, shared (by reference, no copy) across all voices
// that play it. Lives as long as the clip is loaded.
struct loaded_clip
{
    std::vector<uint8_t> pcm;
    ma_format format = ma_format_unknown;
    uint32_t channels = 0;
    uint32_t sample_rate = 0;
    ma_uint64 frame_count = 0;
};

// One active playback. Heap-stable (held by unique_ptr) because miniaudio keeps
// internal pointers to the data source.
struct voice
{
    ma_audio_buffer buffer{};
    ma_sound sound{};
    bool buffer_inited = false;
    bool sound_inited = false;
};

struct audio_renderer::backend
{
    ma_engine engine{};
    std::unordered_map<utils::id, loaded_clip> clips;
    std::unordered_map<utils::id, std::unique_ptr<voice>> voices;
};

audio_renderer::audio_renderer()
    : m_be(std::make_unique<backend>())
{
}

audio_renderer::~audio_renderer()
{
    if (m_enabled)
    {
        stop_all();
        ma_engine_uninit(&m_be->engine);
        m_enabled = false;
    }
}

bool
audio_renderer::init()
{
    ma_engine_config cfg = ma_engine_config_init();
    ma_result r = ma_engine_init(&cfg, &m_be->engine);
    if (r != MA_SUCCESS)
    {
        ALOG_WARN("audio_renderer: no usable audio device (ma_result={}); audio disabled", (int)r);
        m_enabled = false;
        return false;
    }
    m_enabled = true;
    ALOG_INFO("audio_renderer: initialized");
    return true;
}

void
audio_renderer::load_clip(root::audio_clip& clip)
{
    if (!m_enabled)
    {
        return;
    }

    const auto& id = clip.get_id();
    if (m_be->clips.find(id) != m_be->clips.end())
    {
        return;  // already loaded
    }

    auto& data = clip.get_data_buffer();
    if (data.size() == 0)
    {
        ALOG_WARN("audio_renderer: clip {} has no data", id.cstr());
        return;
    }

    // Decode the full clip to PCM once, keeping the source's native format.
    ma_decoder_config dcfg = ma_decoder_config_init_default();
    ma_decoder dec;
    if (ma_decoder_init_memory(data.data(), (size_t)data.size(), &dcfg, &dec) != MA_SUCCESS)
    {
        ALOG_ERROR("audio_renderer: failed to decode clip {}", id.cstr());
        return;
    }

    loaded_clip lc;
    lc.format = dec.outputFormat;
    lc.channels = dec.outputChannels;
    lc.sample_rate = dec.outputSampleRate;

    // Decode in chunks rather than pre-sizing from the reported length: some
    // formats/modes (e.g. Vorbis push decoding) can't report a frame count up
    // front. This is format-agnostic and always correct.
    const ma_uint64 chunk_frames = 4096;
    const ma_uint32 frame_bytes = ma_get_bytes_per_frame(lc.format, lc.channels);
    ma_uint64 total = 0;
    for (;;)
    {
        lc.pcm.resize((size_t)((total + chunk_frames) * frame_bytes));
        ma_uint64 read = 0;
        ma_result rr = ma_decoder_read_pcm_frames(
            &dec, lc.pcm.data() + total * frame_bytes, chunk_frames, &read);
        total += read;
        if (rr != MA_SUCCESS || read < chunk_frames)
        {
            break;
        }
    }
    ma_decoder_uninit(&dec);

    if (total == 0)
    {
        ALOG_ERROR("audio_renderer: clip {} decoded to zero frames", id.cstr());
        return;
    }
    lc.frame_count = total;
    lc.pcm.resize((size_t)(total * frame_bytes));

    m_be->clips.emplace(id, std::move(lc));
}

void
audio_renderer::play(const utils::id& voice_id, const utils::id& clip_id, const play_params& params)
{
    if (!m_enabled)
    {
        return;
    }

    auto clip_it = m_be->clips.find(clip_id);
    if (clip_it == m_be->clips.end())
    {
        ALOG_WARN("audio_renderer: play of unloaded clip {}", clip_id.cstr());
        return;
    }
    auto& lc = clip_it->second;

    // Replace any existing voice with this id.
    stop(voice_id);

    auto v = std::make_unique<voice>();

    // The audio buffer references the clip PCM directly (no copy); the clip
    // outlives the voice because clips are never evicted while playing.
    ma_audio_buffer_config bcfg = ma_audio_buffer_config_init(
        lc.format, lc.channels, lc.frame_count, lc.pcm.data(), nullptr);
    if (ma_audio_buffer_init(&bcfg, &v->buffer) != MA_SUCCESS)
    {
        ALOG_ERROR("audio_renderer: failed to create audio buffer for {}", clip_id.cstr());
        return;
    }
    v->buffer_inited = true;

    if (ma_sound_init_from_data_source(
            &m_be->engine, &v->buffer, 0, nullptr, &v->sound) != MA_SUCCESS)
    {
        ALOG_ERROR("audio_renderer: failed to create sound for {}", clip_id.cstr());
        ma_audio_buffer_uninit(&v->buffer);
        return;
    }
    v->sound_inited = true;

    ma_sound_set_volume(&v->sound, params.volume);
    ma_sound_set_looping(&v->sound, params.looping ? MA_TRUE : MA_FALSE);

    if (params.spatial)
    {
        ma_sound_set_spatialization_enabled(&v->sound, MA_TRUE);
        ma_sound_set_attenuation_model(&v->sound, ma_attenuation_model_linear);
        ma_sound_set_min_distance(&v->sound, params.min_distance);
        ma_sound_set_max_distance(&v->sound, params.max_distance);
        ma_sound_set_position(
            &v->sound, params.position.x, params.position.y, params.position.z);
    }
    else
    {
        ma_sound_set_spatialization_enabled(&v->sound, MA_FALSE);
    }

    ma_sound_start(&v->sound);

    ALOG_INFO("audio_renderer: playing voice {} clip {} (spatial={}, loop={}, vol={}, pos=({},{},{}))",
              voice_id.cstr(),
              clip_id.cstr(),
              params.spatial,
              params.looping,
              params.volume,
              params.position.x,
              params.position.y,
              params.position.z);

    m_be->voices.emplace(voice_id, std::move(v));
}

void
audio_renderer::stop(const utils::id& voice_id)
{
    auto it = m_be->voices.find(voice_id);
    if (it == m_be->voices.end())
    {
        return;
    }
    auto& v = *it->second;
    if (v.sound_inited)
    {
        ma_sound_uninit(&v.sound);
    }
    if (v.buffer_inited)
    {
        ma_audio_buffer_uninit(&v.buffer);
    }
    m_be->voices.erase(it);

    ALOG_INFO("audio_renderer: stopped voice {}", voice_id.cstr());
}

void
audio_renderer::stop_all()
{
    for (auto& [id, v] : m_be->voices)
    {
        if (v->sound_inited)
        {
            ma_sound_uninit(&v->sound);
        }
        if (v->buffer_inited)
        {
            ma_audio_buffer_uninit(&v->buffer);
        }
    }
    m_be->voices.clear();
}

void
audio_renderer::set_voice_position(const utils::id& voice_id, const glm::vec3& pos)
{
    auto it = m_be->voices.find(voice_id);
    if (it == m_be->voices.end())
    {
        return;
    }
    ma_sound_set_position(&it->second->sound, pos.x, pos.y, pos.z);

    ALOG_INFO("audio_renderer: voice {} moved to ({},{},{})", voice_id.cstr(), pos.x, pos.y, pos.z);
}

void
audio_renderer::set_voice_volume(const utils::id& voice_id, float volume)
{
    auto it = m_be->voices.find(voice_id);
    if (it == m_be->voices.end())
    {
        return;
    }
    ma_sound_set_volume(&it->second->sound, volume);
}

void
audio_renderer::set_listener(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up)
{
    if (!m_enabled)
    {
        return;
    }
    ma_engine_listener_set_position(&m_be->engine, 0, pos.x, pos.y, pos.z);
    ma_engine_listener_set_direction(&m_be->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&m_be->engine, 0, up.x, up.y, up.z);
}

void
audio_renderer::tick(float)
{
    if (!m_enabled)
    {
        return;
    }

    // Reap finished one-shot voices so their resources are released.
    for (auto it = m_be->voices.begin(); it != m_be->voices.end();)
    {
        auto& v = *it->second;
        if (!ma_sound_is_looping(&v.sound) && ma_sound_at_end(&v.sound) == MA_TRUE)
        {
            if (v.sound_inited)
            {
                ma_sound_uninit(&v.sound);
            }
            if (v.buffer_inited)
            {
                ma_audio_buffer_uninit(&v.buffer);
            }
            it = m_be->voices.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

}  // namespace audio
}  // namespace kryga
