#pragma once

#include <core/i_processor.h>

#include <cstdint>

namespace kryga
{
namespace render
{
class vulkan_render;
class vulkan_render_loader;
}  // namespace render

namespace render_cmd
{
struct render_command_base;
}  // namespace render_cmd

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
class render_command_processor : public i_processor
{
public:
    render_command_processor(render::vulkan_render& vr, render::vulkan_render_loader& loader)
        : m_vr(vr)
        , m_loader(loader)
    {
    }

    // i_processor::process — execute (and destruct) every command in `frame`'s render
    // queue. Each frame slot holds exactly one frame's commands (the producer is on the
    // other slot), so draining to empty consumes precisely one frame; the caller then
    // issues the draw. `dt` is unused — render's pacing is the main-thread frame gate.
    void
    process(float dt, uint32_t frame) override;

private:
    // Execute (and destruct) one drained command against the renderer/loader. The
    // render twin of audio's apply(message) / physics's apply(message): builds the
    // exec context from the bound refs and runs the central tagged dispatch.
    void
    apply(render_cmd::render_command_base* cmd);

    render::vulkan_render& m_vr;
    render::vulkan_render_loader& m_loader;
};

}  // namespace kryga
