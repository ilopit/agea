#pragma once

#include "packages/base/model/audio_emitter_component.ar.h"

#include "packages/root/model/components/game_object_component.h"
#include "packages/root/model/assets/audio_clip.h"

#include <glm_unofficial/glm.h>

namespace kryga
{
namespace core
{
struct audio_message;
}

namespace base
{

// clang-format off
KRG_ar_class(
    mcp_hint = "Plays an audio clip from this object — 2D or 3D positional using the object "
               "transform as the sound source"
);
class audio_emitter_component : public ::kryga::root::game_object_component
// clang-format on
{
    KRG_gen_meta__audio_emitter_component();

public:
    KRG_gen_class_meta(audio_emitter_component, game_object_component);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(construct_params& c);

    void
    on_tick(float dt) override;

    // Play-mode lifecycle (propagated from the owning game_object). begin_play
    // fires autoplay; end_play stops the voice — this is what makes a looping
    // emitter go quiet on play->edit instead of sounding forever.
    void
    begin_play() override;

    void
    end_play() override;

    // Start the clip now using the current property values. Re-triggering while
    // already playing restarts the voice.
    void
    play();

    void
    stop();

    bool
    is_playing() const;

protected:
    // clang-format off
    KRG_ar_property(
        category     = "Audio",
        access       = all,
        serializable = true,
        instantiate  = share,
        mcp_hint     = "audio clip asset this emitter plays"
    );
    ::kryga::root::audio_clip* m_clip = nullptr;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Audio",
        access       = all,
        serializable = true,
        default      = true,
        mcp_hint     = "playback volume from 0 to 1"
    );
    float m_volume = 1.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Audio",
        access       = all,
        serializable = true,
        default      = true,
        mcp_hint     = "loop the clip until stopped"
    );
    bool m_loop = false;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Audio",
        access       = all,
        serializable = true,
        default      = true,
        mcp_hint     = "start playing automatically the first time the level ticks"
    );
    bool m_autoplay = true;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Audio",
        access       = all,
        serializable = true,
        default      = true,
        mcp_hint     = "3D positional audio attenuated by distance to the listener"
    );
    bool m_spatial = true;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Audio",
        access       = all,
        serializable = true,
        default      = true,
        mcp_hint     = "distance below which the sound plays at full volume"
    );
    float m_min_distance = 1.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Audio",
        access       = all,
        serializable = true,
        default      = true,
        mcp_hint     = "distance beyond which the sound is inaudible"
    );
    float m_max_distance = 100.0f;
    // clang-format on

private:
    // Post an audio intent onto the model queue for audio_bridge to translate.
    // Audio's analog of mark_render_dirty, but scoped to this component — the
    // emitter is the only audio source, so the enqueue lives here, not on the
    // generic game_object_component base.
    void
    emit_audio_message(const ::kryga::core::audio_message& msg) const;

    // Runtime-only: mirrors whether we have a live voice. Set when we emit a
    // play message, cleared on stop. Drives the spatial position-follow in
    // on_tick without querying audio_system (the emitter is decoupled from it).
    bool m_playing = false;

    // Runtime-only: last world position streamed to the voice. on_tick only emits
    // a set_position when the source actually moves, so a stationary spatial
    // emitter doesn't spam the queue every frame. Sentinel forces the first send.
    glm::vec3 m_last_emit_pos = glm::vec3(1e30f);
};

}  // namespace base
}  // namespace kryga
