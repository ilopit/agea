#pragma once

#include <cstdint>

namespace kryga
{

// Consumer-side counterpart to i_translator. Every subsystem (render / physics /
// audio) has exactly ONE engine-owned processor that consumes its SPSC channel.
// One uniform iteration entry point so the engine drives them identically — from a
// worker thread (threaded) or inline (headless):
//
//   process(dt, frame): drain this subsystem's queue and apply it.
//     - dt    : wall-clock seconds since the last call. Self-clocked subsystems
//               (physics fixed-step integration, audio voice reaping) step by it;
//               render ignores it (its pacing is the main-thread frame gate).
//     - frame : the frame slot to consume. Render uses it to pick its double-buffered
//               queue; physics/audio (single ring) ignore it.
//
// Each processor OWNS its queue reference and drains inside process() — the engine
// loop no longer reaches into the channel itself. Anything beyond draining that an
// iteration needs (physics stepping/publishing, audio renderer tick) also happens
// inside process(), keyed off dt.
class i_processor
{
public:
    virtual ~i_processor() = default;

    virtual void
    process(float dt, uint32_t frame) = 0;
};

}  // namespace kryga
