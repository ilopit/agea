#include "render_translator/render_command_processor.h"

#include "render_translator/render_command.h"

#include <core/subsystem_queues.h>

#include <global_state/global_state.h>

namespace kryga
{

void
render_command_processor::drain(uint32_t frame_slot)
{
    render_cmd::render_exec_context exec_ctx{m_vr, m_loader};

    // Drain this frame slot's queue to empty. All the frame's commands were pushed
    // (and made visible via the submitted-counter mutex handoff) before the render
    // thread was released, and the producer is on the other frame slot, so "empty"
    // reliably means "whole frame consumed".
    glob::glob_state()
        .getr_subsystem_queues()
        .render.queue(frame_slot)
        .drain(
            [&exec_ctx](render_cmd::render_command_base*&& cmd)
            {
                // Central tagged dispatch: runs the command for its kind, then
                // destructs it (the arena only rewinds, never calls dtors).
                render_cmd::dispatch(cmd, exec_ctx);
            });
}

}  // namespace kryga
