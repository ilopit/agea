#include "editor_ipc/input_reader.h"

namespace kryga::editor_ipc
{

input_reader::input_reader(frame_header* header, void* region_base)
    : m_header(header)
{
    if (m_header && region_base)
    {
        m_events = reinterpret_cast<input_event*>(
            static_cast<uint8_t*>(region_base) + m_header->input_ring_offset);
    }
}

uint32_t
input_reader::drain(const std::function<void(const input_event&)>& fn, uint32_t max_events)
{
    if (!m_header || !m_events) return 0;

    const uint32_t cap = m_header->input_ring_capacity;
    if (cap == 0) return 0;

    // Load the producer index with acquire so events written before the
    // index bump are visible on this thread.
    uint32_t head = m_header->input_ring_head.load(std::memory_order_acquire);
    uint32_t tail = m_header->input_ring_tail.load(std::memory_order_relaxed);

    uint32_t consumed = 0;
    while (tail != head && consumed < max_events)
    {
        fn(m_events[tail]);
        tail = (tail + 1) % cap;
        ++consumed;
    }

    if (consumed)
    {
        // release store so the producer knows the old slot is free (matters
        // once the ring wraps).
        m_header->input_ring_tail.store(tail, std::memory_order_release);
    }
    return consumed;
}

}  // namespace kryga::editor_ipc
