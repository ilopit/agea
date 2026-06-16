#pragma once

#include <utils/id.h>

#include <glm_unofficial/glm.h>

#include <cstdint>

namespace kryga
{
namespace root
{
class audio_clip;
}

namespace core
{

enum class audio_msg_kind : uint8_t
{
    play,
    stop,
    set_position,
    set_volume,
    set_listener,
};

// Model-emitted audio intent, pushed onto subsystem_queues().audio (via audio_bridge)
// and translated into audio_renderer calls by audio_message_processor. The emitter
// never calls audio_renderer directly — it pushes one of these, mirroring how the
// model marks render dirty instead of touching the GPU.
//
// POD by design: it crosses the model->audio thread boundary BY VALUE through a
// lock-free SPSC ring, so core needs the complete type — but core must NOT depend
// on the audio lib (audio links core; that would cycle). So the playback params
// are inlined here (the processor rebuilds audio::play_params on the audio side) and
// the clip is a raw, model-owned pointer (forward-declared).
//
// Pointer-lifetime hazard: the audio thread dereferences `clip` when it processes
// the message, which is LATER than (and on a different thread from) the emit.
// Safe in practice because clips are package assets that outlive any single frame;
// a clip unloaded between emit and process would dangle. Acceptable for the
// fire-and-forget audio model — revisit if clips become hot-unloadable.
struct audio_message
{
    audio_msg_kind    kind = audio_msg_kind::stop;
    utils::id         voice_id;
    utils::id         clip_id;
    root::audio_clip* clip = nullptr;

    float     volume = 1.0f;
    bool      looping = false;
    bool      spatial = false;
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    float     min_distance = 1.0f;
    float     max_distance = 100.0f;

    // Listener orientation (set_listener only); `position` carries its location.
    glm::vec3 forward = {0.0f, 0.0f, -1.0f};
    glm::vec3 up = {0.0f, 1.0f, 0.0f};
};

}  // namespace core
}  // namespace kryga
