#pragma once

// Engine-side drain for the input ring. The publisher writes to the shm
// region (including the ring); the engine's per-frame tick instantiates an
// input_reader view over the same region and pulls pending events.
//
// Single-consumer: only the engine should hold an input_reader. Construction
// is cheap (stores two pointers); prefer making one per frame rather than
// caching to avoid lifetime dances with the shm region.

#include "editor_ipc/frame_protocol.h"

#include <cstdint>
#include <functional>

namespace kryga::editor_ipc
{

class input_reader
{
public:
    input_reader() = default;

    // `region_base` must point at the same bytes the publisher mmapped as
    // the shm region base; `ring_capacity` must equal
    // `header->input_ring_capacity` (typically INPUT_RING_CAPACITY).
    input_reader(frame_header* header, void* region_base);

    // Drains up to `max_events` events, invoking `fn` for each. Returns the
    // number consumed. The engine's per-frame tick should call this once
    // with a reasonable cap (e.g. 1024) so a malicious / buggy consumer
    // cannot starve the render thread.
    uint32_t
    drain(const std::function<void(const input_event&)>& fn, uint32_t max_events = 1024);

private:
    frame_header* m_header = nullptr;
    input_event* m_events = nullptr;
};

}  // namespace kryga::editor_ipc
