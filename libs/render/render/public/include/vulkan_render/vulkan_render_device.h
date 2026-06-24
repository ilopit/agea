#pragma once

#include "vulkan_render/render_enums.h"
#include "vulkan_render/types/vulkan_generic.h"
#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/utils/vulkan_buffer.h"
#include "vulkan_render/utils/vulkan_image.h"

#include <utils/check.h>
#include <utils/id.h>

#include <functional>
#include <vector>
#include <memory>
#include <mutex>
#include <queue>
#include <deque>
#include <string_view>

struct SDL_Window;

constexpr uint64_t FRAMES_IN_FLIGHT_DEFAULT = 3ULL;
// Upper bound for runtime frames_in_flight. The per-frame frame_data array
// (command buffers, fences, semaphores) is sized to this at init so the
// swapchain image count can grow up to here without reallocating frame_data —
// matches the render_config [1,4] clamp and the UI slider range.
constexpr uint32_t FRAMES_IN_FLIGHT_MAX = 4U;

namespace kryga
{
namespace render
{
namespace vk_utils
{
class descriptor_layout_cache;
class descriptor_allocator;
}  // namespace vk_utils

struct upload_context
{
    VkFence m_upload_fence;
    VkCommandPool m_command_pool;
};

struct frame_data
{
    VkSemaphore m_present_semaphore{};
    VkSemaphore m_render_semaphore{};
    VkFence m_render_fence{};

    VkCommandPool m_command_pool{};
    VkCommandBuffer m_main_command_buffer{};

    std::unique_ptr<vk_utils::descriptor_allocator> m_dynamic_descriptor_allocator{};
};

class render_device
{
public:
    render_device();
    ~render_device();

    struct construct_params
    {
        SDL_Window* window = nullptr;
        bool headless = false;
        // Headless-only: target swapchain dimensions, both required (non-zero).
        // Windowed mode reads the size from `window`.
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t frames_in_flight = static_cast<uint32_t>(FRAMES_IN_FLIGHT_DEFAULT);
        // Explicit swapchain present mode adopted at startup. Defaults to the
        // platform choice; the engine overrides it from render_config.
#if defined(__ANDROID__)
        present_mode present = present_mode::fifo;
#else
        present_mode present = present_mode::mailbox;
#endif
    };

    bool
    construct(construct_params& params);

    bool
    is_headless() const
    {
        return m_headless;
    }

    VkInstance
    vk_instance() const
    {
        return m_vk_instance;
    }

    VkPhysicalDevice
    chosen_GPU() const
    {
        return m_vk_gpu;
    }

    VkDevice
    vk_device() const
    {
        return m_vk_device;
    }

    const VkPhysicalDeviceProperties&
    gpu_properties() const
    {
        return m_gpu_properties;
    }

    VkSurfaceKHR
    vk_surface() const
    {
        return m_surface;
    }

    VmaAllocator
    allocator() const
    {
        return m_allocator;
    }

    VkQueue
    vk_graphics_queue() const
    {
        return m_graphics_queue;
    }

    uint32_t
    graphics_queue_family() const
    {
        return m_graphics_queue_family;
    }

    void
    destruct();

    void
    immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

    uint32_t
    pad_uniform_buffer_size(uint32_t originalSize);

    vk_utils::vulkan_buffer
    create_buffer(size_t alloc_size,
                  VkBufferUsageFlags usage,
                  VmaMemoryUsage memory_usage,
                  VkMemoryPropertyFlags required_flags = 0,
                  std::string_view debug_name = {});

    vk_utils::descriptor_allocator*
    descriptor_allocator()
    {
        return m_descriptor_allocator.get();
    }

    vk_utils::descriptor_layout_cache*
    descriptor_layout_cache()
    {
        return m_descriptor_layout_cache.get();
    }

    frame_data&
    frame(size_t idx)
    {
        return m_frames[idx];
    }

    size_t
    frame_size() const
    {
        return m_frames.size();
    }

    uint32_t
    frames_in_flight() const
    {
        return m_frames_in_flight;
    }

    // Present mode the swapchain was last built with (the requested mode, not a
    // driver fallback). Set at construct from construct_params and on each
    // recreate_swapchain.
    present_mode
    current_present_mode() const
    {
        return m_present_mode;
    }

    struct image_count_range
    {
        uint32_t min = 1;
        uint32_t max = 1;
    };

    // Valid swapchain image count range for `mode` on the current surface, read
    // from the driver. min = surface minImageCount (raised to 3 for mailbox, its
    // non-blocking floor); max = surface maxImageCount (0 = unbounded) clamped to
    // FRAMES_IN_FLIGHT_MAX (our frame_data backing). Windowed only.
    image_count_range
    present_mode_image_range(present_mode mode) const;

    // Whether the surface supports `mode`. FIFO is guaranteed by spec; mailbox /
    // immediate are optional. Windowed only.
    bool
    is_present_mode_supported(present_mode mode) const;

    // --- Render→display latency (VK_KHR_present_wait) -----------------------
    // Measures submit→displayed time per present. Only active when the device
    // enabled VK_KHR_present_wait + present_id at creation (windowed + driver
    // support); otherwise these are inert and present_latency_ms() stays 0.

    bool
    present_wait_supported() const
    {
        return m_present_wait_supported;
    }

    // Stamp this frame's present and return the id to chain via VkPresentIdKHR,
    // or nullptr when unsupported (caller then presents without an id). The
    // returned pointer stays valid until the next call.
    const uint64_t*
    begin_present_timing();

    // Non-blocking poll of completed presents; updates the latency EMA. Call once
    // per presented frame, right after vkQueuePresentKHR.
    void
    poll_present_timing();

    // Present-wait pacing. Blocks until all but `pace_frames` of the submitted
    // presents have been displayed, bounding how far the CPU runs ahead of the
    // scanout (kills the FIFO render-ahead queue — the dominant present latency).
    // No-op when pace_frames == 0 or VK_KHR_present_wait is unsupported. Call
    // once per frame BEFORE acquiring the next swapchain image. Best-effort: a
    // finite timeout keeps the render thread from hanging if the display stalls.
    void
    wait_present_pacing(uint32_t pace_frames);

    // Exponential moving average of submit→displayed latency in milliseconds.
    // 0 when unsupported or before the first sample.
    float
    present_latency_ms() const
    {
        return m_present_latency_ms;
    }

    void
    switch_frame_indeces()
    {
        ++m_current_frame_number;
        // Cycle over the in-flight count. The renderer keeps the invariant
        // frames_in_flight == swapchain image count (see recreate_swapchain), so
        // this also equals m_swapchain_images.size().
        m_current_frame_index = m_current_frame_number % m_frames_in_flight;
    }

    // Rebuild the swapchain so it holds `desired_count` images (clamped to the
    // surface's supported range), then set frames_in_flight to the actual image
    // count — preserving the engine-wide invariant frames_in_flight == image
    // count. Windowed devices only. `rebuild_framebuffers` is invoked with the
    // NEW images/views while both old and new exist, so callers can rebuild any
    // render-pass framebuffers that wrap swapchain images (main/composite)
    // before the old swapchain is destroyed. Returns the actual image count.
    // `mode` is the explicit present mode to build with; mailbox is bumped to
    // >=3 images by the driver, which this preserves via the invariant.
    uint32_t
    recreate_swapchain(
        uint32_t desired_count,
        present_mode mode,
        const std::function<void(const std::vector<vk_utils::vulkan_image_sptr>&,
                                 const std::vector<vk_utils::vulkan_image_view_sptr>&)>&
            rebuild_framebuffers);

    frame_data&
    get_current_frame()
    {
        return m_frames[m_current_frame_index];
    }

    uint64_t
    get_current_frame_index() const
    {
        return m_current_frame_index;
    }

    uint64_t
    get_current_frame_number() const
    {
        return m_current_frame_number;
    }

    // Swapchain image index that draw_main last acquired/presented. Used by
    // out-of-band readers (screenshot capture) to sample the image that is
    // actually on screen — which is the acquired index, NOT frame_slot % count
    // (they diverge under MAILBOX when the presentation engine hands back an
    // image index unequal to the frame slot).
    uint32_t
    last_presented_image_index() const
    {
        return m_last_presented_image_index;
    }

    void
    set_last_presented_image_index(uint32_t idx)
    {
        m_last_presented_image_index = idx;
    }

    // Render-finished semaphore for a specific ACQUIRED swapchain image (not the
    // frame slot). Signalled by that frame's submit, waited on by present. Keyed
    // by image so the present wait stays correct even when the presentation
    // engine returns an image index != frame slot (MAILBOX).
    VkSemaphore
    image_render_semaphore(uint32_t image_index)
    {
        return m_image_render_semaphores[image_index];
    }

    // Fence of the frame that last rendered into this image (or VK_NULL_HANDLE).
    // draw_main waits on it after acquiring, before reusing the image, so a new
    // frame never renders into an image a prior frame still owns.
    VkFence&
    image_in_flight_fence(uint32_t image_index)
    {
        return m_images_in_flight[image_index];
    }

    VkSwapchainKHR&
    swapchain()
    {
        return m_swapchain;
    }

    void
    flush_command_buffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);

    vk_device_provider
    get_vk_device_provider();

    vma_allocator_provider
    get_vma_allocator_provider();

    void
    wait_for_fences();

    VkFormat
    get_swapchain_format() const
    {
        return m_swapchain_image_format;
    }

    VkFormat
    get_depth_format() const
    {
        return m_depth_format;
    }

    // Swapchain image extent. Equal to the display-visible size — the
    // presentation engine composites any rotation at present time.
    VkExtent2D
    swapchain_extent() const
    {
        return m_swapchain_extent;
    }

    std::vector<vk_utils::vulkan_image_sptr>
    get_swapchain_images()
    {
        return m_swapchain_images;
    }

    std::vector<vk_utils::vulkan_image_view_sptr>
    get_swapchain_image_views()
    {
        return m_swapchain_image_views;
    }

    struct memory_stats
    {
        uint64_t device_total = 0;
        uint64_t device_used = 0;
        uint64_t host_total = 0;
        uint64_t host_used = 0;
        uint32_t allocation_count = 0;
    };

    memory_stats
    get_memory_stats() const;

    void
    log_memory_stats() const;

    using delayed_deleter = std::function<void(VkDevice vkd, VmaAllocator va)>;

    void
    schedule_to_delete(delayed_deleter d);

    void
    delete_immediately(const delayed_deleter& d);

    void
    delete_scheduled_actions();

    void
    flush_deferred_deletions();

    // private:
    bool
    init_vulkan(SDL_Window* window, bool headless);
    bool
    deinit_vulkan();

    bool
    init_swapchain(bool headless, uint32_t width, uint32_t height);
    bool
    deinit_swapchain();

    // Reconcile the requested (config) present mode + image count against what
    // the surface actually supports, falling back when they aren't applicable:
    // requested mode -> mailbox (lowest latency) -> fifo (guaranteed by spec).
    // The count is clamped into the chosen mode's valid range. Windowed only;
    // sets m_present_mode and m_frames_in_flight. Call after the surface exists,
    // before init_swapchain.
    void
    resolve_present_config(present_mode requested_mode, uint32_t requested_count);

    bool
    init_commands();
    bool
    deinit_commands();

    bool
    init_sync_structures();
    bool
    deinit_sync_structures();

    bool
    init_descriptors();
    bool
    deinit_descriptors();

    std::unique_ptr<vk_utils::descriptor_allocator> m_descriptor_allocator;
    std::unique_ptr<vk_utils::descriptor_layout_cache> m_descriptor_layout_cache;

    VkInstance m_vk_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_msg = VK_NULL_HANDLE;
    VkPhysicalDevice m_vk_gpu = VK_NULL_HANDLE;
    VkDevice m_vk_device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties m_gpu_properties{};
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VmaAllocator m_allocator = VK_NULL_HANDLE;
    VkQueue m_graphics_queue{};
    uint32_t m_graphics_queue_family{};
    upload_context m_upload_context{};

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchain_image_format = VK_FORMAT_UNDEFINED;

    VkExtent2D m_swapchain_extent{0, 0};

    std::vector<vk_utils::vulkan_image_sptr> m_swapchain_images;
    std::vector<vk_utils::vulkan_image_view_sptr> m_swapchain_image_views;

    // Per-swapchain-image present synchronization. Sized to the swapchain image
    // count and rebuilt on recreate_swapchain. Render semaphores are owned;
    // the in-flight fences are non-owning copies of frame-slot fences.
    std::vector<VkSemaphore> m_image_render_semaphores;
    std::vector<VkFence> m_images_in_flight;

    std::vector<frame_data> m_frames;

    // the format for the depth image
    VkFormat m_depth_format;

    bool m_headless = false;
    uint32_t m_frames_in_flight = static_cast<uint32_t>(FRAMES_IN_FLIGHT_DEFAULT);
    present_mode m_present_mode = present_mode::mailbox;

    // VK_KHR_present_wait latency tracking (see begin/poll_present_timing).
    // vkWaitForPresentKHR isn't exported by the loader .lib (newer extension), so
    // it's resolved via vkGetDeviceProcAddr when the extension is enabled.
    bool m_present_wait_supported = false;
    PFN_vkWaitForPresentKHR m_vk_wait_for_present = nullptr;
    uint64_t m_present_id = 0;          // per-swapchain, strictly increasing
    uint64_t m_current_present_id = 0;  // storage chained into VkPresentIdKHR
    struct present_stamp
    {
        uint64_t id;
        uint64_t submit_ns;
    };
    std::deque<present_stamp> m_present_pending;
    float m_present_latency_ms = 0.0f;

    uint64_t m_current_frame_number = UINT64_MAX;
    uint64_t m_current_frame_index = 0ULL;
    uint32_t m_last_presented_image_index = 0U;

    struct delayed_delete_action
    {
        uint64_t frame_idx = 0;
        delayed_deleter del = nullptr;
    };

    struct delayed_delete_action_compare
    {
        bool
        operator()(const delayed_delete_action& l, const delayed_delete_action& r)
        {
            return l.frame_idx > r.frame_idx;
        }
    };

    std::priority_queue<delayed_delete_action,
                        std::vector<delayed_delete_action>,
                        delayed_delete_action_compare>
        m_delayed_delete_queue;

    // Guards m_delayed_delete_queue. The streaming render loop lets the main
    // thread enqueue deferred deletions (level reload, material/texture
    // teardown, render-config rebuilds) while the render thread is draining its
    // own queue and running delete_scheduled_actions — without this they race
    // and corrupt the heap. Deleters run OUTSIDE the lock (see the .cpp) so an
    // unlucky deleter that schedules more work can't self-deadlock.
    std::mutex m_delete_mutex;
};

}  // namespace render
}  // namespace kryga