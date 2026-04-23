#pragma once

// Engine-side writer for the shared-memory frame protocol.
//
// Owns the shm region plus a single reusable host-visible staging buffer.
// Every call to publish() issues a GPU copy from `src_image` into the
// staging buffer via immediate_submit(), then memcpies the staging bytes
// into the next free slot of the shm region and updates the atomics.
//
// The VK-touching pieces live in frame_publisher.cpp so this header has no
// Vulkan dependency; consumers of the addon reuse this same lib but skip
// frame_publisher entirely.

#include "editor_ipc/frame_protocol.h"
#include "editor_ipc/named_event.h"
#include "editor_ipc/shared_memory.h"

#include <vulkan/vulkan.h>

#include <functional>
#include <string>

namespace kryga::render
{
class render_device;
}

namespace kryga::editor_ipc
{

class frame_publisher
{
public:
    struct config
    {
        std::string name;      // Bare identifier; see editor/README.md.
        uint32_t max_width = 0;
        uint32_t max_height = 0;
        pixel_format format = pf_rgba8;
    };

    frame_publisher() = default;
    ~frame_publisher();

    frame_publisher(const frame_publisher&) = delete;
    frame_publisher& operator=(const frame_publisher&) = delete;

    // Allocates the shm region and the staging buffer. Must be called from
    // the thread that owns the render_device. Returns true on success.
    bool
    init(render::render_device& device, const config& cfg);

    // Releases GPU staging resources and the shm region.
    void
    shutdown(render::render_device& device);

    // Copies `src_image` (which must have been created with
    // VK_IMAGE_USAGE_TRANSFER_SRC_BIT) into the next free slot and bumps the
    // atomics. `src_layout` is the image's current layout — the internal
    // copy command transitions it to TRANSFER_SRC_OPTIMAL and back.
    //
    // `src_format` is the format of the image; the method handles the
    // BGRA → RGBA swizzle when necessary so the consumer always sees the
    // format it was configured with.
    //
    // Must be called from the same thread as init(). Blocks until the GPU
    // copy has completed.
    void
    publish(render::render_device& device,
            VkImage src_image,
            VkImageLayout src_layout,
            VkFormat src_format,
            uint32_t width,
            uint32_t height);

    // Drain up to `max_events` queued input events from the ring and pass
    // each to `fn`. Returns the number drained. Non-blocking; safe to call
    // every frame even when the ring is empty.
    uint32_t
    drain_input(const std::function<void(const input_event&)>& fn, uint32_t max_events = 1024);

    bool
    is_open() const
    {
        return m_shm.is_open();
    }

    const std::string&
    last_error() const
    {
        return m_last_error;
    }

private:
    frame_header*
    header()
    {
        return static_cast<frame_header*>(m_shm.data());
    }

    // Returns a slot index different from both latest_ready_slot and
    // reading_slot. With NUM_SLOTS = 3 such a slot always exists.
    uint32_t
    pick_write_slot() const;

    shared_memory m_shm;
    region_layout m_layout{};
    config m_cfg{};
    std::string m_last_error;

    // Staging buffer for GPU → CPU image copy. Host-visible, host-coherent.
    // Sized for max_width * max_height * 4 bytes.
    VkBuffer m_staging_buffer = VK_NULL_HANDLE;
    void* m_staging_allocation = nullptr;  // VmaAllocation, erased to keep this header VMA-free.
    void* m_staging_mapped = nullptr;
    size_t m_staging_size = 0;

    // Phase 2: consumers wait on this after every publish() instead of
    // polling. The name is derived from the channel: "<name>_fr".
    named_event m_frame_ready;
};

}  // namespace kryga::editor_ipc
