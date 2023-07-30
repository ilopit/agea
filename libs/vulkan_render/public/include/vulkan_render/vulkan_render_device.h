#pragma once

#include "vulkan_render/types/vulkan_generic.h"
#include "vulkan_render/types/vulkan_render_types_fwds.h"
#include "vulkan_render/utils/vulkan_buffer.h"
#include "vulkan_render/utils/vulkan_image.h"

#include <utils/singleton_instance.h>
#include <utils/id.h>

#include <functional>
#include <vector>
#include <memory>

struct SDL_Window;

constexpr uint64_t FRAMES_IN_FLYIGNT = 3ULL;

namespace agea
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
    VkSemaphore m_present_semaphore;
    VkSemaphore m_render_semaphore;
    VkFence m_render_fence;

    VkCommandPool m_command_pool;
    VkCommandBuffer m_main_command_buffer;

    std::unique_ptr<vk_utils::descriptor_allocator> m_dynamic_descriptor_allocator;
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
    };

    bool
    construct(construct_params& params);

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
                  VkMemoryPropertyFlags required_flags = 0);

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

    void
    switch_frame_indeces()
    {
        ++m_current_frame_number;
        m_current_frame_index = m_current_frame_number % m_frames.size();
    }

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

    VkSwapchainKHR&
    swapchain()
    {
        return m_swapchain;
    }

    vk_device_provider
    get_vk_device_provider();

    vma_allocator_provider
    get_vma_allocator_provider();

    void
    wait_for_fences();

    VkFormat
    get_swapchain_format() const
    {
        return m_swachain_image_format;
    }

    VkFormat
    get_depth_format() const
    {
        return m_depth_format;
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

    using vk_deleter = std::function<void(VkDevice vkd)>;

    void
    schedule_to_delete(vk_deleter d)
    {
        d(m_vk_device);
    }

    using vma_deleter = std::function<void(VmaAllocator va)>;

    void
    schedule_to_delete(vma_deleter d)
    {
        d(m_allocator);
    }

private:
    bool
    init_vulkan(SDL_Window* window, bool headless);
    bool
    deinit_vulkan();

    bool
    init_swapchain(bool headless, uint32_t width, uint32_t height);
    bool
    deinit_swapchain();

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
    VkPhysicalDeviceProperties m_gpu_properties;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VmaAllocator m_allocator = VK_NULL_HANDLE;
    VkQueue m_graphics_queue;
    uint32_t m_graphics_queue_family;
    upload_context m_upload_context;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swachain_image_format = VK_FORMAT_UNDEFINED;

    std::vector<vk_utils::vulkan_image_sptr> m_swapchain_images;
    std::vector<vk_utils::vulkan_image_view_sptr> m_swapchain_image_views;

    std::vector<frame_data> m_frames;

    VkDescriptorSetLayout m_single_texture_set_layout = VK_NULL_HANDLE;

    // the format for the depth image
    VkFormat m_depth_format;

    uint64_t m_current_frame_number = UINT64_MAX;
    uint64_t m_current_frame_index = 0ULL;
};

}  // namespace render

namespace glob
{
struct render_device
    : public ::agea::singleton_instance<::agea::render::render_device, render_device>
{
};
}  // namespace glob

}  // namespace agea