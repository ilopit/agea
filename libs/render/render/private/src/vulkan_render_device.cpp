#include "vulkan_render/vulkan_render_device.h"

#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/vk_pipeline_builder.h"

#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/utils/vulkan_debug.h"

#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"

#include <native/native_window.h>

#include <utils/process.h>
#include <utils/file_utils.h>

#include <global_state/global_state.h>

#include <VkBootstrap.h>
#include <SDL.h>
#include <SDL_vulkan.h>

#include <iostream>
#include <fstream>
#include <string>

namespace kryga
{

void
state_mutator__render_device::set(gs::state& s)
{
    auto p = s.create_box<render::render_device>("render_device");
    s.m_render_device = p;
}

namespace render
{

render_device::render_device() = default;

render_device::~render_device() = default;

bool
render_device::construct(construct_params& params)
{
    m_headless = params.headless;

    auto width =
        params.headless ? 1024U : (uint32_t)glob::glob_state().get_native_window()->get_size().w;
    auto height =
        params.headless ? 1024U : (uint32_t)glob::glob_state().get_native_window()->get_size().h;

    init_vulkan(params.window, params.headless);

    init_swapchain(params.headless, width, height);

    init_commands();

    init_sync_structures();

    init_descriptors();

    return true;
}

void
render_device::destruct()
{
    if (m_vk_device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_vk_device);
    }

    // Flush all pending delayed deletions before tearing down
    while (!m_delayed_delete_queue.empty())
    {
        auto action = std::move(m_delayed_delete_queue.top());
        m_delayed_delete_queue.pop();
        action.del(m_vk_device, m_allocator);
    }

    for (auto& frame : m_frames)
    {
        frame.m_dynamic_descriptor_allocator->cleanup();
    }

    m_descriptor_allocator->cleanup();
    m_descriptor_layout_cache->cleanup();

    deinit_sync_structures();

    deinit_commands();

    deinit_swapchain();

    deinit_descriptors();

    deinit_vulkan();
}

bool
render_device::init_vulkan(SDL_Window* window, bool headless)
{
    vkb::InstanceBuilder builder;

    // make the vulkan instance
#if KRYGA_VULKAN_DEBUG
    auto inst_ret =
        builder.set_app_name("KRYGA")
            .request_validation_layers(true)
            .set_debug_callback(&vk_utils::debug_messenger_callback)
            .set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            .set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
            .set_headless(headless)
            .require_api_version(1, 2)
            .build();
#else
    auto inst_ret = builder.set_app_name("KRYGA")
                        .request_validation_layers(false)
                        .set_headless(headless)
                        .require_api_version(1, 2)
                        .build();
#endif

    vkb::Instance vkb_inst = inst_ret.value();

    // grab the instance
    m_vk_instance = vkb_inst.instance;
    m_debug_msg = vkb_inst.debug_messenger;

#if KRYGA_VULKAN_DEBUG
    vk_utils::debug_init(m_vk_instance);
#endif

    if (!headless)
    {
        SDL_Vulkan_CreateSurface(window, m_vk_instance, &m_surface);
    }

    // use vkbootstrap to select a gpu.
    // We want a gpu that can write to the SDL surface and supports vulkan 1.2
    vkb::PhysicalDeviceSelector selector{vkb_inst};

    VkPhysicalDeviceFeatures features_11{};
    features_11.fillModeNonSolid = true;
    // shaderInt64 not required — BDA uses uvec2 via GL_EXT_buffer_reference_uvec2

    selector.set_required_features(features_11)
        .set_minimum_version(1, 2)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete);

    if (!headless)
    {
        selector.set_surface(m_surface);
    }

    vkb::PhysicalDevice physicalDevice = selector.select().value();
    // create the final vulkan device

    vkb::DeviceBuilder deviceBuilder{physicalDevice};

    // Enable descriptor indexing features for bindless textures
    VkPhysicalDeviceDescriptorIndexingFeatures indexing_features{};
    indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexing_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    indexing_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    indexing_features.descriptorBindingPartiallyBound = VK_TRUE;
    indexing_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    indexing_features.runtimeDescriptorArray = VK_TRUE;
    deviceBuilder.add_pNext(&indexing_features);

    // Enable buffer device address for BDA-based buffer access
    VkPhysicalDeviceBufferDeviceAddressFeatures bda_features{};
    bda_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bda_features.bufferDeviceAddress = VK_TRUE;
    deviceBuilder.add_pNext(&bda_features);

    // Enable scalar block layout for natural C++ struct alignment in GPU buffers
    VkPhysicalDeviceScalarBlockLayoutFeatures scalar_features{};
    scalar_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES;
    scalar_features.scalarBlockLayout = VK_TRUE;
    deviceBuilder.add_pNext(&scalar_features);

    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    m_vk_device = vkbDevice.device;
    m_vk_gpu = physicalDevice.physical_device;

    // use vkbootstrap to get a Graphics queue
    m_graphics_queue = vkbDevice.get_queue(vkb::QueueType::graphics).value();

    m_graphics_queue_family = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = m_vk_gpu;
    allocatorInfo.device = m_vk_device;
    allocatorInfo.instance = m_vk_instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &m_allocator);

    vkGetPhysicalDeviceProperties(m_vk_gpu, &m_gpu_properties);

    KRG_VK_NAME(m_vk_device, m_vk_device, "kryga.device");
    KRG_VK_NAME(m_vk_device, m_graphics_queue, "kryga.graphics_queue");

    return true;
}

bool
render_device::deinit_vulkan()
{
    vmaDestroyAllocator(m_allocator);

    vkDestroyDevice(m_vk_device, nullptr);

    if (m_debug_msg != VK_NULL_HANDLE)
    {
        vkb::destroy_debug_utils_messenger(m_vk_instance, m_debug_msg);
    }

    vkDestroyInstance(m_vk_instance, nullptr);

    return true;
}

bool
render_device::init_swapchain(bool headless, uint32_t width, uint32_t height)
{
    m_swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;
    // hardcoding the depth format to 32 bit float
    m_depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;

    if (!headless)
    {
        vkb::SwapchainBuilder swapchain_builder{m_vk_gpu, m_vk_device, m_surface};

        VkSurfaceFormatKHR format{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

        vkb::Swapchain vkb_swapchain = swapchain_builder.use_default_format_selection()
                                           .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
                                           .set_desired_extent(width, height)
                                           .set_desired_format(format)
                                           .build()
                                           .value();

        // store swapchain and its related images
        m_swapchain = vkb_swapchain.swapchain;

        auto images = vkb_swapchain.get_images().value();
        for (size_t idx = 0; idx < images.size(); ++idx)
        {
            auto himg = vk_utils::vulkan_image::create(images[idx], vkb_swapchain.image_format);
            KRG_VK_NAME_FMT(m_vk_device, images[idx], "swapchain.image_{}", idx);
            m_swapchain_images.push_back(std::make_shared<vk_utils::vulkan_image>(std::move(himg)));
        }

        auto views = vkb_swapchain.get_image_views().value();
        for (size_t idx = 0; idx < views.size(); ++idx)
        {
            KRG_VK_NAME_FMT(m_vk_device, views[idx], "swapchain.view_{}", idx);
            m_swapchain_image_views.push_back(
                vk_utils::vulkan_image_view::create_shared(std::move(views[idx])));
        }

        m_swapchain_image_format = vkb_swapchain.image_format;
    }
    else
    {
        // depth image size will match the window
        VkExtent3D swapchain_image_extent = {width, height, 1};

        auto simg_info = vk_utils::make_image_create_info(
            m_swapchain_image_format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            swapchain_image_extent);

        // for the depth image, we want to allocate it from gpu local memory
        VmaAllocationCreateInfo simg_allocinfo = {};
        simg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        simg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_swapchain_images.resize(FRAMES_IN_FLIGHT);
        m_swapchain_image_views.resize(FRAMES_IN_FLIGHT);
        for (auto i = 0; i < m_swapchain_images.size(); ++i)
        {
            // allocate and create the image
            m_swapchain_images[i] =
                std::make_shared<vk_utils::vulkan_image>(vk_utils::vulkan_image::create(
                    get_vma_allocator_provider(), simg_info, simg_allocinfo));

            // build a image-view for the depth image to use for rendering
            auto swapchain_image_view_ci =
                vk_utils::make_imageview_create_info(m_swapchain_image_format,
                                                     m_swapchain_images[i]->image(),
                                                     VK_IMAGE_ASPECT_COLOR_BIT);

            m_swapchain_image_views[i] =
                vk_utils::vulkan_image_view::create_shared(swapchain_image_view_ci);
        }
    }

    m_frames.resize(m_swapchain_images.size());

    return true;
}

bool
render_device::deinit_swapchain()
{
    m_swapchain_image_views.clear();

    if (m_swapchain)
    {
        vkDestroySwapchainKHR(m_vk_device, m_swapchain, nullptr);
    }

    if (m_surface)
    {
        vkDestroySurfaceKHR(m_vk_instance, m_surface, nullptr);
    }
    else
    {
        m_swapchain_images.clear();
    }

    return true;
}

bool
render_device::init_commands()
{
    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command buffers
    auto command_pool_ci = vk_utils::make_command_pool_create_info(
        m_graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (size_t i = 0; i < m_frames.size(); ++i)
    {
        auto& frame = m_frames[i];
        VK_CHECK(
            vkCreateCommandPool(m_vk_device, &command_pool_ci, nullptr, &frame.m_command_pool));

        // allocate the default command buffer that we will use for rendering
        auto command_buffer_ai =
            vk_utils::make_command_buffer_allocate_info(frame.m_command_pool, 1);

        VK_CHECK(vkAllocateCommandBuffers(
            m_vk_device, &command_buffer_ai, &frame.m_main_command_buffer));

        KRG_VK_NAME_FMT(m_vk_device, frame.m_command_pool, "frame_{}.cmd_pool", i);
        KRG_VK_NAME_FMT(m_vk_device, frame.m_main_command_buffer, "frame_{}.main_cmd", i);
    }

    auto upload_command_pool_ci = vk_utils::make_command_pool_create_info(m_graphics_queue_family);
    // create pool for upload context
    VK_CHECK(vkCreateCommandPool(
        m_vk_device, &upload_command_pool_ci, nullptr, &m_upload_context.m_command_pool));
    KRG_VK_NAME(m_vk_device, m_upload_context.m_command_pool, "upload.cmd_pool");

    return true;
}

bool
render_device::deinit_commands()
{
    for (auto& f : m_frames)
    {
        vkDestroyCommandPool(m_vk_device, f.m_command_pool, nullptr);
    }

    vkDestroyCommandPool(m_vk_device, m_upload_context.m_command_pool, nullptr);

    return true;
}

bool
render_device::init_sync_structures()
{
    // create syncronization structures
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to syncronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first frame
    auto fenceCreateInfo = vk_utils::make_fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    VkSemaphoreCreateInfo semaphoreCreateInfo = vk_utils::make_semaphore_create_info();

    for (size_t i = 0; i < m_frames.size(); ++i)
    {
        auto& frame = m_frames[i];
        VK_CHECK(vkCreateFence(m_vk_device, &fenceCreateInfo, nullptr, &frame.m_render_fence));

        VK_CHECK(vkCreateSemaphore(
            m_vk_device, &semaphoreCreateInfo, nullptr, &frame.m_present_semaphore));
        VK_CHECK(vkCreateSemaphore(
            m_vk_device, &semaphoreCreateInfo, nullptr, &frame.m_render_semaphore));

        KRG_VK_NAME_FMT(m_vk_device, frame.m_render_fence, "frame_{}.render_fence", i);
        KRG_VK_NAME_FMT(m_vk_device, frame.m_present_semaphore, "frame_{}.present_sem", i);
        KRG_VK_NAME_FMT(m_vk_device, frame.m_render_semaphore, "frame_{}.render_sem", i);
    }

    VkFenceCreateInfo uploadFenceCreateInfo = vk_utils::make_fence_create_info();

    VK_CHECK(vkCreateFence(
        m_vk_device, &uploadFenceCreateInfo, nullptr, &m_upload_context.m_upload_fence));
    KRG_VK_NAME(m_vk_device, m_upload_context.m_upload_fence, "upload.fence");
    return true;
}

bool
render_device::deinit_sync_structures()
{
    vkDestroyFence(m_vk_device, m_upload_context.m_upload_fence, nullptr);

    for (auto& f : m_frames)
    {
        vkDestroyFence(m_vk_device, f.m_render_fence, nullptr);
        vkDestroySemaphore(m_vk_device, f.m_present_semaphore, nullptr);
        vkDestroySemaphore(m_vk_device, f.m_render_semaphore, nullptr);
    }
    return true;
}

bool
render_device::init_descriptors()
{
    m_descriptor_allocator = std::make_unique<vk_utils::descriptor_allocator>();
    m_descriptor_layout_cache = std::make_unique<vk_utils::descriptor_layout_cache>();

    for (auto& frame : m_frames)
    {
        frame.m_dynamic_descriptor_allocator = std::make_unique<vk_utils::descriptor_allocator>();
    }
    return true;
}

bool
render_device::deinit_descriptors()
{
    m_descriptor_layout_cache.reset();
    m_descriptor_allocator.reset();

    return true;
}

void
render_device::flush_command_buffer(VkCommandBuffer command_buffer,
                                    VkQueue queue,
                                    bool free /*= true*/)
{
    if (command_buffer == VK_NULL_HANDLE)
    {
        return;
    }

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submitInfo = vk_utils::make_submit_info(&command_buffer);
    submitInfo.commandBufferCount = 1;
    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceInfo = vk_utils::make_fence_create_info();
    VkFence fence;
    vkCreateFence(m_vk_device, &fenceInfo, nullptr, &fence);
    // Submit to the queue
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    // Wait for the fence to signal that command buffer has finished executing
    vkWaitForFences(m_vk_device, 1, &fence, VK_TRUE, 100000000000);
    vkDestroyFence(m_vk_device, fence, nullptr);
    if (free)
    {
        vkFreeCommandBuffers(m_vk_device, m_upload_context.m_command_pool, 1, &command_buffer);
    }
}

vk_device_provider
render_device::get_vk_device_provider()
{
    return []() { return glob::glob_state().get_render_device()->vk_device(); };
}

vma_allocator_provider
render_device::get_vma_allocator_provider()
{
    return []() { return glob::glob_state().get_render_device()->allocator(); };
}

void
render_device::wait_for_fences()
{
    for (auto& frame : m_frames)
    {
        vkWaitForFences(m_vk_device, 1, &frame.m_render_fence, true, 1'000'000'000);
    }
}

void
render_device::schedule_to_delete(delayed_deleter d)
{
    m_delayed_delete_queue.push({m_current_frame_number + FRAMES_IN_FLIGHT, std::move(d)});
}

void
render_device::delete_immediately(delayed_deleter d)
{
    d(m_vk_device, m_allocator);
}

void
render_device::delete_scheduled_actions()
{
    if (m_delayed_delete_queue.empty())
    {
        return;
    }

    auto device = glob::glob_state().get_render_device();
    auto current_frame = device->get_current_frame_number();
    while (!m_delayed_delete_queue.empty() &&
           m_delayed_delete_queue.top().frame_idx <= current_frame)
    {
        m_delayed_delete_queue.pop();
    }
}

void
render_device::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    // allocate the default command buffer that we will use for rendering
    auto command_buffer_ai =
        vk_utils::make_command_buffer_allocate_info(m_upload_context.m_command_pool, 1);

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(m_vk_device, &command_buffer_ai, &cmd));

    // begin the command buffer recording. We will use this command buffer exactly once, so we
    // want to let vulkan know that
    auto command_buffer_bi =
        vk_utils::make_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &command_buffer_bi));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    auto submit = vk_utils::make_submit_info(&cmd);

    // submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(m_graphics_queue, 1, &submit, m_upload_context.m_upload_fence));

    vkWaitForFences(m_vk_device, 1, &m_upload_context.m_upload_fence, true, 9999999999);
    vkResetFences(m_vk_device, 1, &m_upload_context.m_upload_fence);

    vkResetCommandPool(m_vk_device, m_upload_context.m_command_pool, 0);
}

uint32_t
render_device::pad_uniform_buffer_size(uint32_t original_size)
{
    // Calculate required alignment based on minimum device offset alignment
    uint32_t min_ubo_alignment = (uint32_t)m_gpu_properties.limits.minUniformBufferOffsetAlignment;
    uint32_t aligned_size = original_size;
    if (min_ubo_alignment > 0)
    {
        aligned_size = (aligned_size + min_ubo_alignment - 1) & ~(min_ubo_alignment - 1);
    }
    return aligned_size;
}

vk_utils::vulkan_buffer
render_device::create_buffer(size_t alloc_size,
                             VkBufferUsageFlags usage,
                             VmaMemoryUsage memory_usage,
                             VkMemoryPropertyFlags required_flags,
                             std::string_view debug_name)
{
    // allocate vertex buffer
    VkBufferCreateInfo buffer_ci = {};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.pNext = nullptr;
    buffer_ci.size = alloc_size;

    // Add BDA flag for buffers that may be accessed via device address
    if (usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))
    {
        usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    buffer_ci.usage = usage;

    // let the VMA library know that this data should be writable by CPU, but also readable by
    // GPU
    VmaAllocationCreateInfo vma_alloc_ci = {};
    vma_alloc_ci.usage = memory_usage;
    vma_alloc_ci.requiredFlags = required_flags;

    // allocate the buffer

    return vk_utils::vulkan_buffer::create(buffer_ci, vma_alloc_ci, debug_name);
}

}  // namespace render
}  // namespace kryga
