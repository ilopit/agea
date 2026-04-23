#pragma once

// Kryga editor IPC — shared-memory frame protocol (Phase 1).
//
// A single shared-memory region is laid out as:
//
//   [ frame_header ] [ pad ] [ slot0 pixels ] [ slot1 pixels ] [ slot2 pixels ]
//
// All multi-byte atomics live inside frame_header; slot_pixels blobs are
// read/written without per-byte locking — correctness is upheld by the
// triple-buffer protocol below.
//
// Triple-buffer protocol (single-producer / single-consumer):
//
//   - Publisher (engine) owns `latest_ready_slot`. It picks a slot that is
//     not equal to `latest_ready_slot` AND not equal to `reading_slot`,
//     writes pixel data into that slot's pixel blob, then atomically stores
//     the slot index into `latest_ready_slot` (release) and increments
//     `frame_counter` (release).
//
//   - Consumer (editor) samples `latest_ready_slot` (acquire). If it changed
//     since the last read, consumer atomically writes that index into
//     `reading_slot` (so the publisher won't pick it next), reads the pixels,
//     then writes `SLOT_NONE` back into `reading_slot`.
//
//   With three slots, the publisher can always find a slot that is neither
//   ready nor being read (3 − 2 ≥ 1), which makes the protocol lock-free on
//   the publisher side.
//
// Nothing in this header pulls in Vulkan, Node-API, or OS specifics — it is
// shared verbatim between the engine's publisher and the editor's consumer.

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace kryga::editor_ipc
{

// Four-byte magic "KRGA".
inline constexpr uint32_t SHM_MAGIC = 0x4B524741u;

// Bump when any field layout in frame_header changes. Publisher and consumer
// refuse to attach across a version mismatch.
inline constexpr uint32_t SHM_VERSION = 1;

inline constexpr uint32_t NUM_SLOTS = 3;
inline constexpr uint32_t SLOT_NONE = 0xFFFFFFFFu;

enum pixel_format : uint32_t
{
    pf_rgba8 = 0,
    pf_bgra8 = 1,
};

// Fixed-size, standard-layout header. Reader and writer both mmap the same
// region; all cross-process coordination lives in the atomic members below.
struct frame_header
{
    // --- Immutable, written once by the publisher on region create ---
    uint32_t magic;          // SHM_MAGIC
    uint32_t version;        // SHM_VERSION
    uint32_t max_width;      // Provisioned pixel width per slot
    uint32_t max_height;     // Provisioned pixel height per slot
    uint32_t pixel_format;   // pixel_format enum value
    uint32_t stride_bytes;   // Row stride at max_width (always max_width * 4 for pf_rgba8/bgra8)
    uint32_t num_slots;      // NUM_SLOTS (3)
    uint32_t slot_bytes;     // stride_bytes * max_height, bytes per slot
    uint64_t slot_offsets[NUM_SLOTS];  // Byte offsets from region base to each slot

    // --- Mutable; accessed concurrently by publisher and consumer ---

    // Bumps when max_width/max_height/pixel_format change (resize handshake
    // in Phase 3). Consumer refuses a frame whose generation does not match
    // the one it sampled with the header.
    std::atomic<uint32_t> generation;

    // Monotonic total frames published. Consumer uses (prev < current) to
    // detect a new frame without re-reading pixel data.
    std::atomic<uint64_t> frame_counter;

    // Index (0..NUM_SLOTS-1) of the slot that currently holds the most
    // recently published frame, or SLOT_NONE if no frame has been published
    // yet. Written release, read acquire.
    std::atomic<uint32_t> latest_ready_slot;

    // Index of the slot the consumer is currently reading from, or
    // SLOT_NONE. Publisher never picks this slot as its next write target.
    std::atomic<uint32_t> reading_slot;

    // Publisher's most recent per-frame dimensions. May be smaller than
    // max_width/max_height during normal operation (e.g. viewport shrink);
    // consumer uses these when uploading the texture.
    std::atomic<uint32_t> current_width;
    std::atomic<uint32_t> current_height;

    // Liveness / attach state, polled by both sides at ~1 Hz. Used by the
    // consumer to show a "disconnected" badge without waiting for a timeout
    // and by the engine to know whether to bother publishing.
    std::atomic<uint32_t> publisher_alive;   // 0 / 1
    std::atomic<uint32_t> consumer_attached; // 0 / 1

    // Reserved for Phase 2 (pointer to input ring). Ignored by Phase 1.
    uint64_t input_ring_offset;
    uint32_t input_ring_size;
    uint32_t _reserved0;
};

static_assert(std::is_standard_layout_v<frame_header>,
              "frame_header must be standard-layout for cross-process mapping");

// The region size is fixed at creation. Cache-align the slot blobs so
// concurrent updates on one slot don't thrash another slot's cache lines.
inline constexpr size_t CACHE_LINE = 64;

inline constexpr size_t
align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

// Computes the total byte size of a region provisioned for (W × H × 4-byte
// pixels) × NUM_SLOTS. Also returns the slot_offsets that frame_header must
// be populated with.
struct region_layout
{
    size_t total_bytes;
    uint64_t slot_offsets[NUM_SLOTS];
    uint32_t stride_bytes;
    uint32_t slot_bytes;
};

inline region_layout
compute_region_layout(uint32_t max_width, uint32_t max_height)
{
    region_layout out{};
    out.stride_bytes = max_width * 4;
    out.slot_bytes = out.stride_bytes * max_height;

    size_t cursor = align_up(sizeof(frame_header), CACHE_LINE);
    for (uint32_t i = 0; i < NUM_SLOTS; ++i)
    {
        out.slot_offsets[i] = cursor;
        cursor += align_up(out.slot_bytes, CACHE_LINE);
    }
    out.total_bytes = cursor;
    return out;
}

}  // namespace kryga::editor_ipc
