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
};

// Model-emitted audio intent, drained each frame by audio_bridge and translated
// into audio_system calls. The emitter never calls audio_system directly — it
// pushes one of these, mirroring how the model marks render dirty instead of
// touching the GPU.
//
// POD by design: it lives in a std::vector on core::queues::model, so core needs
// the complete type — but core must NOT depend on the audio lib (audio links
// core; that would cycle). So the playback params are inlined here (the bridge
// rebuilds audio::play_params on the audio side) and the clip is a raw,
// model-owned pointer (forward-declared). The pointer is valid because messages
// are produced and drained within the same frame on the main thread, and the
// clip is an asset that outlives the frame.
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
};

}  // namespace core
}  // namespace kryga
