#pragma once

#include <audio/audio_renderer.h>

#include <global_state/system.h>

#include <span>
#include <string_view>

namespace kryga
{
namespace audio
{

// Thin gs::system wrapper around audio_renderer — the audio analog of render_system
// (which wraps vulkan_render). It owns the renderer and forwards the global_state
// lifecycle; all playback logic lives in audio_renderer, and message translation
// lives in the processor. Reached via glob_state().get_audio_system()->renderer.
class audio_system : public gs::system
{
public:
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

    // Bring up the renderer's audio device. Non-fatal on a deviceless host.
    void
    on_init(gs::state&) override;

    audio_renderer renderer;
};

}  // namespace audio
}  // namespace kryga
