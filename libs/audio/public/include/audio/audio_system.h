#pragma once

#include <global_state/system.h>

#include <glm_unofficial/glm.h>

#include <utils/id.h>

#include <memory>
#include <span>
#include <string_view>
#include <vector>

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

// Audio playback subsystem built on miniaudio. Lives in global_state and is
// ticked on the main thread. It is a pure consumer of model state: emitter
// components push positions/volumes in via the voice API; the listener pose is
// pushed in from the active camera. It never reads the render layer.
//
// If the platform has no usable audio device (headless / CI), init fails
// gracefully and every call becomes a no-op — audio is non-essential and must
// not take the app down.
class audio_system : public gs::system
{
public:
    audio_system();
    ~audio_system();

    std::string_view
    name() const override
    {
        return "audio";
    }

    std::span<const std::string_view>
    deps() const override
    {
        return {};
    }

    void
    on_init(gs::state&) override;

    bool
    is_enabled() const
    {
        return m_enabled;
    }

    // Decode a clip's encoded bytes into cached PCM, keyed by the clip id.
    // Idempotent — a clip already loaded is left untouched.
    void
    load_clip(root::audio_clip& clip);

    bool
    is_clip_loaded(const utils::id& clip_id) const;

    // Start clip_id on the logical voice keyed by voice_id (typically the
    // emitter's id). Replaces any voice already playing under that id.
    void
    play(const utils::id& voice_id, const utils::id& clip_id, const play_params& params);

    void
    stop(const utils::id& voice_id);

    void
    stop_all();

    bool
    is_voice_playing(const utils::id& voice_id) const;

    // Append the ids of all currently-live voices. Used by audio_bridge to reap
    // voices whose owning emitter is gone from the model cache.
    void
    collect_active_voice_ids(std::vector<utils::id>& out) const;

    void
    set_voice_position(const utils::id& voice_id, const glm::vec3& pos);

    void
    set_voice_volume(const utils::id& voice_id, float volume);

    // Listener pose for spatial audio — usually driven by the active camera.
    void
    set_listener(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up);

    void
    set_master_volume(float v);

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
