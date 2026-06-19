#pragma once

#include <cstdint>

namespace kryga
{
namespace render
{
class vulkan_render;
class vulkan_render_loader;
}  // namespace render

// Consumer-side processor for the render command channel: drains one frame slot's
// queue and executes each command against the renderer/loader. The render analog of
// audio_message_processor (audio) and physics_command_processor (physics) — every
// subsystem now has ONE engine-owned consumer object, driven by its worker thread
// (threaded) or the inline tick (headless), instead of an ephemeral local.
//
// Unlike the other two it is STATELESS beyond the renderer/loader binding: render
// keeps no accumulator / active set — the command stream IS the whole state, which
// is why this was previously a free function. Sole render-command consumer once
// render access is handed to the render thread (or the main thread in headless).
class render_command_processor
{
public:
    render_command_processor(render::vulkan_render& vr, render::vulkan_render_loader& loader)
        : m_vr(vr)
        , m_loader(loader)
    {
    }

    // Execute (and destruct) every command in frame_slot's render queue. Each frame
    // slot holds exactly one frame's commands (the producer is on the other slot), so
    // draining to empty consumes precisely one frame; the caller then issues the draw.
    void
    drain(uint32_t frame_slot);

private:
    render::vulkan_render& m_vr;
    render::vulkan_render_loader& m_loader;
};

}  // namespace kryga
