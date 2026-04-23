// shmdump — debug CLI that attaches to a running editor IPC region, prints
// the header atomics, and optionally saves the most recent ready slot to a
// PNG. Build target: editor_ipc_shmdump (executable).
//
// Usage:
//   editor_ipc_shmdump <name> <max_width> <max_height> [out.png]
//
// Example:
//   editor_ipc_shmdump default 1024 1024 frame.png
//
// Exit codes: 0 on success, 1 on attach failure, 2 on bad args.
// Intentionally terse — this is a diagnostic, not a product.

#include "editor_ipc/frame_protocol.h"
#include "editor_ipc/shared_memory.h"

#include <render/utils/image_compare.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace kryga::editor_ipc;

int
main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::fprintf(stderr,
                     "usage: %s <name> <max_width> <max_height> [out.png]\n",
                     argv[0]);
        return 2;
    }

    const char* name = argv[1];
    const uint32_t w = static_cast<uint32_t>(std::atoi(argv[2]));
    const uint32_t h = static_cast<uint32_t>(std::atoi(argv[3]));
    const char* out_png = (argc >= 5) ? argv[4] : nullptr;

    const auto layout = compute_region_layout(w, h);

    shared_memory shm;
    if (!shm.open(name, shared_memory::mode::attach, layout.total_bytes))
    {
        std::fprintf(stderr, "attach failed: %s\n", shm.last_error().c_str());
        return 1;
    }

    auto* hdr = static_cast<frame_header*>(shm.data());
    if (hdr->magic != SHM_MAGIC)
    {
        std::fprintf(stderr, "bad magic: expected %08x got %08x\n", SHM_MAGIC, hdr->magic);
        return 1;
    }

    std::printf("  region       : %s  (%zu bytes)\n", name, layout.total_bytes);
    std::printf("  magic/version: %08x / %u\n", hdr->magic, hdr->version);
    std::printf("  max dims     : %ux%u  format=%u  stride=%u  slot_bytes=%u\n",
                hdr->max_width, hdr->max_height, hdr->pixel_format,
                hdr->stride_bytes, hdr->slot_bytes);
    std::printf("  current dims : %ux%u\n",
                hdr->current_width.load(std::memory_order_acquire),
                hdr->current_height.load(std::memory_order_acquire));
    std::printf("  generation   : %u\n", hdr->generation.load(std::memory_order_acquire));
    std::printf("  frame_counter: %llu\n",
                static_cast<unsigned long long>(
                    hdr->frame_counter.load(std::memory_order_acquire)));
    const uint32_t ready = hdr->latest_ready_slot.load(std::memory_order_acquire);
    const uint32_t reading = hdr->reading_slot.load(std::memory_order_acquire);
    std::printf("  latest_ready : %s\n",
                ready == SLOT_NONE ? "(none)" : std::to_string(ready).c_str());
    std::printf("  reading      : %s\n",
                reading == SLOT_NONE ? "(none)" : std::to_string(reading).c_str());
    std::printf("  publisher    : %s\n",
                hdr->publisher_alive.load(std::memory_order_acquire) ? "alive" : "gone");
    std::printf("  consumer     : %s\n",
                hdr->consumer_attached.load(std::memory_order_acquire) ? "attached" : "none");
    std::printf("  input ring   : head=%u tail=%u cap=%u\n",
                hdr->input_ring_head.load(std::memory_order_acquire),
                hdr->input_ring_tail.load(std::memory_order_acquire),
                hdr->input_ring_capacity);

    if (!out_png)
    {
        return 0;
    }

    if (ready == SLOT_NONE)
    {
        std::fprintf(stderr, "no frame to dump (latest_ready == NONE)\n");
        return 1;
    }

    // Claim the slot so the publisher won't overwrite mid-read, copy out,
    // then release. Race with a very fast publisher is possible but
    // bounded — if the publisher swapped just before our claim, ready
    // already pointed at a different slot than the one we read. Accept
    // that: shmdump is a debug tool, not a guaranteed-consistent capture.
    hdr->reading_slot.store(ready, std::memory_order_release);
    const uint8_t* slot =
        static_cast<const uint8_t*>(shm.data()) + hdr->slot_offsets[ready];
    const uint32_t cw = hdr->current_width.load(std::memory_order_acquire);
    const uint32_t ch = hdr->current_height.load(std::memory_order_acquire);

    std::string out_path(out_png);
    const bool ok = kryga::render::save_png(out_path, slot, cw, ch);
    hdr->reading_slot.store(SLOT_NONE, std::memory_order_release);

    if (!ok)
    {
        std::fprintf(stderr, "save_png failed for %s\n", out_png);
        return 1;
    }
    std::printf("  dumped slot %u (%ux%u) to %s\n", ready, cw, ch, out_png);
    return 0;
}
