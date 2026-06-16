#include "audio/audio_system.h"

#include <global_state/global_state.h>

namespace kryga
{

void
state_mutator__audio_system::set(gs::state& s)
{
    auto p = s.create_box<audio::audio_system>("audio_system");
    s.m_audio_system = p;
    s.register_system(p);
}

namespace audio
{

void
audio_system::on_init(gs::state&)
{
    renderer.init();
}

}  // namespace audio
}  // namespace kryga
