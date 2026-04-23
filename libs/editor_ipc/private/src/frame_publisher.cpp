#include "editor_ipc/frame_publisher.h"
#include "editor_ipc/input_reader.h"

#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/utils/vulkan_initializers.h>

#include <utils/kryga_log.h>

#include <vk_mem_alloc.h>

#include <cstring>

namespace kryga::editor_ipc
{

bool
frame_publisher::init(render::render_device& device, const config& cfg)
{
    m_cfg = cfg;

    if (m_cfg.max_width == 0 || m_cfg.max_height == 0)
    {
        m_last_error = "frame_publisher: max_width / max_height must be nonzero";
        return false;
    }

    m_layout = compute_region_layout(m_cfg.max_width, m_cfg.max_height);

    // Sweep stale region before creating — a crash from a previous run may
    // have left a region with our name in place (POSIX only; Windows
    // mappings are refcounted).
    shared_memory::unlink_stale(m_cfg.name);

    if (!m_shm.open(m_cfg.name, shared_memory::mode::create, m_layout.total_bytes))
    {
        m_last_error = "frame_publisher: " + m_shm.last_error();
        return false;
    }

    // Zero the whole region and populate the immutable header fields.
    std::memset(m_shm.data(), 0, m_shm.size());

    auto* h = header();
    h->magic = SHM_MAGIC;
    h->version = SHM_VERSION;
    h->max_width = m_cfg.max_width;
    h->max_height = m_cfg.max_height;
    h->pixel_format = static_cast<uint32_t>(m_cfg.format);
    h->stride_bytes = m_layout.stride_bytes;
    h->num_slots = NUM_SLOTS;
    h->slot_bytes = m_layout.slot_bytes;
    for (uint32_t i = 0; i < NUM_SLOTS; ++i)
    {
        h->slot_offsets[i] = m_layout.slot_offsets[i];
    }

    h->generation.store(1, std::memory_order_release);
    h->frame_counter.store(0, std::memory_order_release);
    h->latest_ready_slot.store(SLOT_NONE, std::memory_order_release);
    h->reading_slot.store(SLOT_NONE, std::memory_order_release);
    h->current_width.store(m_cfg.max_width, std::memory_order_release);
    h->current_height.store(m_cfg.max_height, std::memory_order_release);
    h->publisher_alive.store(1, std::memory_order_release);
    h->consumer_attached.store(0, std::memory_order_release);
    h->input_ring_offset = m_layout.input_ring_offset;
    h->input_ring_capacity = INPUT_RING_CAPACITY;
    h->input_ring_head.store(0, std::memory_order_release);
    h->input_ring_tail.store(0, std::memory_order_release);

    // frame_ready event used by the addon in its worker thread.
    if (!m_frame_ready.open(m_cfg.name + "_fr", named_event::mode::create))
    {
        m_last_error = "frame_publisher: " + m_frame_ready.last_error();
        m_shm.close();
        return false;
    }

    // Allocate the staging buffer: host-visible, host-coherent, large enough
    // for one full frame at max resolution.
    m_staging_size = static_cast<size_t>(m_cfg.max_width) * m_cfg.max_height * 4;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = m_staging_size;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    aci.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    aci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocation alloc = nullptr;
    VmaAllocationInfo ai{};
    auto res = vmaCreateBuffer(device.allocator(), &bci, &aci, &m_staging_buffer, &alloc, &ai);
    if (res != VK_SUCCESS)
    {
        m_last_error = "frame_publisher: vmaCreateBuffer failed";
        m_shm.close();
        return false;
    }

    m_staging_allocation = alloc;
    m_staging_mapped = ai.pMappedData;

    ALOG_INFO("frame_publisher: opened '{}' ({}x{}, region={} bytes, slot={} bytes)",
              m_cfg.name,
              m_cfg.max_width,
              m_cfg.max_height,
              m_layout.total_bytes,
              m_layout.slot_bytes);
    return true;
}

void
frame_publisher::shutdown(render::render_device& device)
{
    if (auto* h = header())
    {
        h->publisher_alive.store(0, std::memory_order_release);
    }

    // Kick the consumer once so it sees publisher_alive == 0 immediately
    // instead of waiting out a poll interval.
    m_frame_ready.signal();
    m_frame_ready.close();

    if (m_staging_buffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(
            device.allocator(),
            m_staging_buffer,
            static_cast<VmaAllocation>(m_staging_allocation));
        m_staging_buffer = VK_NULL_HANDLE;
        m_staging_allocation = nullptr;
        m_staging_mapped = nullptr;
        m_staging_size = 0;
    }

    m_shm.close();
}

uint32_t
frame_publisher::drain_input(const std::function<void(const input_event&)>& fn,
                             uint32_t max_events)
{
    if (!is_open()) return 0;
    input_reader reader(header(), m_shm.data());
    return reader.drain(fn, max_events);
}

uint32_t
frame_publisher::pick_write_slot() const
{
    // Snapshot both "occupied" slot indices. Either or both may be SLOT_NONE.
    const auto ready = header()->latest_ready_slot.load(std::memory_order_acquire);
    const auto reading = header()->reading_slot.load(std::memory_order_acquire);
    for (uint32_t i = 0; i < NUM_SLOTS; ++i)
    {
        if (i != ready && i != reading)
        {
            return i;
        }
    }
    // Unreachable with NUM_SLOTS ≥ 3; keep a sane fallback instead of aborting.
    return 0;
}

frame_publisher::~frame_publisher()
{
    // Intentionally does NOT tear down VK resources — the owner must call
    // shutdown() while the device is still alive. A null staging_buffer is
    // the expected state at this point.
}

void
frame_publisher::publish(render::render_device& device,
                         VkImage src_image,
                         VkImageLayout src_layout,
                         VkFormat src_format,
                         uint32_t width,
                         uint32_t height)
{
    KRG_check(is_open(), "frame_publisher::publish called before init");
    KRG_check(width <= m_cfg.max_width && height <= m_cfg.max_height,
              "frame_publisher::publish: frame exceeds provisioned size");

    const uint32_t slot = pick_write_slot();

    device.immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            if (src_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            {
                render::vk_utils::make_insert_image_memory_barrier(
                    cmd,
                    src_image,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    src_layout,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
            }

            VkBufferImageCopy copy{};
            copy.bufferOffset = 0;
            copy.bufferRowLength = 0;    // tightly packed at `width` texels
            copy.bufferImageHeight = 0;  // tightly packed
            copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.mipLevel = 0;
            copy.imageSubresource.baseArrayLayer = 0;
            copy.imageSubresource.layerCount = 1;
            copy.imageOffset = {0, 0, 0};
            copy.imageExtent = {width, height, 1};

            vkCmdCopyImageToBuffer(cmd,
                                   src_image,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   m_staging_buffer,
                                   1,
                                   &copy);

            // Restore the source image's original layout so the next frame's
            // render graph sees the same starting layout it expected.
            if (src_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            {
                render::vk_utils::make_insert_image_memory_barrier(
                    cmd,
                    src_image,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    src_layout,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
            }
        });

    // immediate_submit blocks on the fence, so staging_mapped holds the final
    // pixels once we get here. Copy into the chosen slot, applying the BGRA
    // → RGBA swizzle if the source image is BGRA but the consumer expects
    // RGBA (the cross-platform default).
    const auto* src = static_cast<const uint8_t*>(m_staging_mapped);
    auto* dst =
        static_cast<uint8_t*>(m_shm.data()) + m_layout.slot_offsets[slot];

    const size_t bytes = static_cast<size_t>(width) * height * 4;
    const bool needs_swizzle = (m_cfg.format == pf_rgba8) &&
                               (src_format == VK_FORMAT_B8G8R8A8_UNORM ||
                                src_format == VK_FORMAT_B8G8R8A8_SRGB);

    if (needs_swizzle)
    {
        const uint32_t* s = reinterpret_cast<const uint32_t*>(src);
        uint32_t* d = reinterpret_cast<uint32_t*>(dst);
        const size_t n = bytes / 4;
        for (size_t i = 0; i < n; ++i)
        {
            const uint32_t p = s[i];
            // BGRA (little-endian: B,G,R,A in memory, stored as 0xAARRGGBB)
            // → RGBA (R,G,B,A in memory, stored as 0xAABBGGRR).
            d[i] = (p & 0xFF00FF00u) | ((p & 0x00FF0000u) >> 16) | ((p & 0x000000FFu) << 16);
        }
    }
    else
    {
        std::memcpy(dst, src, bytes);
    }

    auto* h = header();
    h->current_width.store(width, std::memory_order_release);
    h->current_height.store(height, std::memory_order_release);

    // Publish: update latest_ready and bump the frame counter. The counter
    // is a tie-breaker for consumers that miss a slot update.
    h->latest_ready_slot.store(slot, std::memory_order_release);
    h->frame_counter.fetch_add(1, std::memory_order_acq_rel);

    // Signal the consumer's worker thread. Auto-reset: one wait consumes
    // one signal. If the consumer is backlogged (wait hasn't returned yet),
    // the next signal is coalesced — that is fine, we only ever want the
    // consumer to know "a new frame is available" not "N frames are queued".
    m_frame_ready.signal();
}

}  // namespace kryga::editor_ipc
