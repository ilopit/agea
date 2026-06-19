#pragma once

#include <glm_unofficial/glm.h>

#include <utils/id.h>

#include <memory>
#include <string_view>

namespace kryga
{
namespace root
{
class audio_clip;
}

namespace audio
{

struct play_params
{
    float volume = 1.0f;
    bool looping = false;

    // When true the voice is positioned in 3D and attenuated relative to the
    // listener; when false it plays as a flat 2D sound (UI, music).
    bool spatial = false;
    glm::vec3 position = {0.0f, 0.0f, 0.0f};

    // Linear attenuation range for spatial voices.
    float min_distance = 1.0f;
    float max_distance = 100.0f;
};

// The audio playback engine, built on miniaudio — the audio analog of vulkan_render
// (the renderer): all the real work lives here, while audio_system is just the thin
// gs::system wrapper that owns one of these. It manages decoded clips and active
// voices and drives miniaudio; it knows NOTHING about core::audio_message (the model
// intent format) — translating messages onto this API is the processor's job, exactly
// as vulkan_render knows nothing about render commands.
//
// Owned and called exclusively by the audio thread (the audio worker in
// engine_threads), except in headless mode where the main thread drives it
// single-threaded. It is a pure consumer of model state and never reads the render
// layer.
//
// If the platform has no usable audio device (headless / CI), init() fails gracefully
// and every call becomes a no-op — audio is non-essential and must not take the app
// down.
class audio_renderer
{
public:
    audio_renderer();
    ~audio_renderer();

    audio_renderer(const audio_renderer&) = delete;
    audio_renderer&
    operator=(const audio_renderer&) = delete;

    // Bring up the miniaudio device. Returns whether audio is enabled (false on a
    // headless/deviceless host, in which case every other call is a no-op).
    bool
    init();

    bool
    is_enabled() const
    {
        return m_enabled;
    }

    // Decode a clip's encoded bytes into cached PCM, keyed by the clip id.
    // Idempotent — a clip already loaded is left untouched.
    void
    load_clip(root::audio_clip& clip);

    // Start clip_id on the logical voice keyed by voice_id (typically the
    // emitter's id). Replaces any voice already playing under that id.
    void
    play(const utils::id& voice_id, const utils::id& clip_id, const play_params& params);

    void
    stop(const utils::id& voice_id);

    void
    stop_all();

    void
    set_voice_position(const utils::id& voice_id, const glm::vec3& pos);

    void
    set_voice_volume(const utils::id& voice_id, float volume);

    // Listener pose for spatial audio — usually driven by the active camera.
    void
    set_listener(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up);

    // Per-frame: advances the engine clock bookkeeping and reaps finished voices.
    void
    tick(float dt);

private:
    struct backend;
    std::unique_ptr<backend> m_be;
    bool m_enabled = false;
};

}  // namespace audio
}  // namespace kryga
