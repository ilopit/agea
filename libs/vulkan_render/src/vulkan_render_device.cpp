#include "vulkan_render/vulkan_render_device.h"

#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/vk_pipeline_builder.h"
#include "vulkan_render/shader_reflection.h"

#include "vulkan_render/utils/vulkan_initializers.h"

#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"

#include <native/native_window.h>

#include <utils/process.h>
#include <utils/file_utils.h>

#include <resource_locator/resource_locator.h>

#include <VkBootstrap.h>
#include <SDL.h>
#include <SDL_vulkan.h>

#include <iostream>
#include <fstream>

namespace agea
{

glob::render_device::type glob::render_device::type::s_instance;

namespace render
{

render_device::render_device() = default;

render_device::~render_device() = default;

bool
render_device::construct(construct_params& params)
{
    init_vulkan(params.window);

    init_swapchain();

    init_default_renderpass();

    init_framebuffers();

    init_commands();

    init_sync_structures();

    init_descriptors();

    return true;
}

void
render_device::destruct()
{
    for (auto& frame : m_frames)
    {
        frame.m_dynamic_descriptor_allocator->cleanup();
    }

    m_descriptor_allocator->cleanup();
    m_descriptor_layout_cache->cleanup();

    deinit_sync_structures();

    deinit_commands();

    deinit_framebuffers();

    deinit_default_renderpass();

    deinit_swapchain();

    deinit_descriptors();

    deinit_vulkan();
}

bool
render_device::init_vulkan(SDL_Window* window)
{
    vkb::InstanceBuilder builder;

    // make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("AGEA")
                        .request_validation_layers(true)
                        .use_default_debug_messenger()
                        .require_api_version(1, 2)
                        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    // grab the instance
    m_vk_instance = vkb_inst.instance;
    m_debug_msg = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(window, m_vk_instance, &m_surface);

    // use vkbootstrap to select a gpu.
    // We want a gpu that can write to the SDL surface and supports vulkan 1.2
    vkb::PhysicalDeviceSelector selector{vkb_inst};

    VkPhysicalDeviceFeatures features_11{};
    features_11.fillModeNonSolid = true;

    selector.set_required_features(features_11)
        .set_minimum_version(1, 2)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete);

    selector.set_surface(m_surface);

    vkb::PhysicalDevice physicalDevice = selector.select().value();
    // create the final vulkan device

    vkb::DeviceBuilder deviceBuilder{physicalDevice};

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
    vmaCreateAllocator(&allocatorInfo, &m_allocator);

    vkGetPhysicalDeviceProperties(m_vk_gpu, &m_gpu_properties);

    return true;
}

bool
render_device::deinit_vulkan()
{
    vmaDestroyAllocator(m_allocator);

    vkDestroyDevice(m_vk_device, nullptr);

    vkb::destroy_debug_utils_messenger(m_vk_instance, m_debug_msg);

    vkDestroyInstance(m_vk_instance, nullptr);

    return true;
}

bool
render_device::init_swapchain()
{
    m_swachain_image_format = VK_FORMAT_B8G8R8A8_UNORM;
    // hardcoding the depth format to 32 bit float
    m_depth_format = VK_FORMAT_D32_SFLOAT;

    vkb::SwapchainBuilder swapchain_builder{m_vk_gpu, m_vk_device, m_surface};

    auto width = (uint32_t)glob::native_window::get()->get_size().w;
    auto height = (uint32_t)glob::native_window::get()->get_size().h;

    VkSurfaceFormatKHR format{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

    vkb::Swapchain vkb_swapchain = swapchain_builder
                                       .use_default_format_selection()
                                       // use vsync present mode
                                       .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
                                       .set_desired_extent(width, height)
                                       .set_desired_format(format)
                                       .build()
                                       .value();

    // store swapchain and its related images
    m_swapchain = vkb_swapchain.swapchain;
    m_swapchain_images = vkb_swapchain.get_images().value();
    m_swapchain_image_views = vkb_swapchain.get_image_views().value();

    m_swachain_image_format = vkb_swapchain.image_format;

    m_depth_image_views.resize(m_swapchain_image_views.size());
    m_depth_images.resize(m_swapchain_images.size());

    // depth image size will match the window
    VkExtent3D depth_image_extent = {width, height, 1};

    // the depth image will be a image with the format we selected and Depth Attachment usage flag
    VkImageCreateInfo dimg_info = vk_utils::make_image_create_info(
        m_depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depth_image_extent);

    // for the depth image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    for (auto i = 0; i < m_swapchain_images.size(); ++i)
    {
        // allocate and create the image
        m_depth_images[i] =
            vk_utils::vulkan_image::create(get_vma_allocator_provider(), dimg_info, dimg_allocinfo);

        // build a image-view for the depth image to use for rendering
        VkImageViewCreateInfo depth_image_view_ci = vk_utils::make_imageview_create_info(
            m_depth_format, m_depth_images[i].image(), VK_IMAGE_ASPECT_DEPTH_BIT);

        VK_CHECK(
            vkCreateImageView(m_vk_device, &depth_image_view_ci, nullptr, &m_depth_image_views[i]));
    }

    return true;
}

bool
render_device::deinit_swapchain()
{
    for (auto i : m_swapchain_image_views)
    {
        vkDestroyImageView(m_vk_device, i, nullptr);
    }

    for (auto i : m_depth_image_views)
    {
        vkDestroyImageView(m_vk_device, i, nullptr);
    }

    m_depth_images.clear();
    m_swapchain_image_views.clear();

    vkDestroySwapchainKHR(m_vk_device, m_swapchain, nullptr);
    vkDestroySurfaceKHR(m_vk_instance, m_surface, nullptr);

    return true;
}

bool
render_device::init_default_renderpass()
{
    // we define an attachment description for our main color image
    // the attachment is loaded as "clear" when renderpass start
    // the attachment is stored when renderpass ends
    // the attachment layout starts as "undefined", and transitions to "Present" so its possible to
    // display it we dont care about stencil, and dont use multisampling

    VkAttachmentDescription color_attachment = {};
    color_attachment.format = m_swachain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth_attachment = {};
    // Depth attachment
    depth_attachment.flags = 0;
    depth_attachment.format = m_depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // we are going to create 1 subpass, which is the minimum you can do
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    // hook the depth attachment into the subpass
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    // 1 dependency, which is from "outside" into the subpass. And we can read or write color
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // array of 2 attachments, one for the color, and other for depth
    VkAttachmentDescription attachments[2] = {color_attachment, depth_attachment};

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    // 2 attachments from said array
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = &attachments[0];
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    // render_pass_info.dependencyCount = 1;
    // render_pass_info.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(m_vk_device, &render_pass_info, nullptr, &m_render_pass));

    return true;
}

bool
render_device::deinit_default_renderpass()
{
    vkDestroyRenderPass(m_vk_device, m_render_pass, nullptr);

    return true;
}

bool
render_device::init_framebuffers()
{
    // create the framebuffers for the swapchain images. This will connect the render-pass to the
    // images for rendering
    m_frames.resize(FRAMES_IN_FLYIGNT);

    auto width = (uint32_t)glob::native_window::get()->get_size().w;
    auto height = (uint32_t)glob::native_window::get()->get_size().h;

    VkFramebufferCreateInfo fb_info =
        vk_utils::make_framebuffer_create_info(m_render_pass, VkExtent2D{width, height});

    const uint32_t swapchain_imagecount = (uint32_t)m_swapchain_images.size();
    m_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

    for (uint32_t i = 0; i < swapchain_imagecount; i++)
    {
        VkImageView attachments[2] = {m_swapchain_image_views[i], m_depth_image_views[i]};

        fb_info.pAttachments = attachments;
        fb_info.attachmentCount = 2;
        VK_CHECK(vkCreateFramebuffer(m_vk_device, &fb_info, nullptr, &m_framebuffers[i]));
    }
    return true;
}

bool
render_device::deinit_framebuffers()
{
    const uint32_t swapchain_imagecount = (uint32_t)m_swapchain_images.size();
    for (uint32_t i = 0; i < swapchain_imagecount; i++)
    {
        vkDestroyFramebuffer(m_vk_device, m_framebuffers[i], nullptr);
    }

    m_frames.clear();

    return true;
}

bool
render_device::init_commands()
{
    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo command_pool_ci = vk_utils::make_command_pool_create_info(
        m_graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (auto& frame : m_frames)
    {
        VK_CHECK(
            vkCreateCommandPool(m_vk_device, &command_pool_ci, nullptr, &frame.m_command_pool));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo command_buffer_ai =
            vk_utils::make_command_buffer_allocate_info(frame.m_command_pool, 1);

        VK_CHECK(vkAllocateCommandBuffers(m_vk_device, &command_buffer_ai,
                                          &frame.m_main_command_buffer));
    }

    VkCommandPoolCreateInfo upload_command_pool_ci =
        vk_utils::make_command_pool_create_info(m_graphics_queue_family);
    // create pool for upload context
    VK_CHECK(vkCreateCommandPool(m_vk_device, &upload_command_pool_ci, nullptr,
                                 &m_upload_context.m_command_pool));

    return true;
}

bool
render_device::deinit_commands()
{
    for (int i = 0; i < FRAMES_IN_FLYIGNT; i++)
    {
        vkDestroyCommandPool(m_vk_device, m_frames[i].m_command_pool, nullptr);
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
    VkFenceCreateInfo fenceCreateInfo =
        vk_utils::make_fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    VkSemaphoreCreateInfo semaphoreCreateInfo = vk_utils::make_semaphore_create_info();

    for (auto& frame : m_frames)
    {
        VK_CHECK(vkCreateFence(m_vk_device, &fenceCreateInfo, nullptr, &frame.m_render_fence));

        VK_CHECK(vkCreateSemaphore(m_vk_device, &semaphoreCreateInfo, nullptr,
                                   &frame.m_present_semaphore));
        VK_CHECK(vkCreateSemaphore(m_vk_device, &semaphoreCreateInfo, nullptr,
                                   &frame.m_render_semaphore));
    }

    VkFenceCreateInfo uploadFenceCreateInfo = vk_utils::make_fence_create_info();

    VK_CHECK(vkCreateFence(m_vk_device, &uploadFenceCreateInfo, nullptr,
                           &m_upload_context.m_upload_fence));
    return true;
}

bool
render_device::deinit_sync_structures()
{
    vkDestroyFence(m_vk_device, m_upload_context.m_upload_fence, nullptr);

    for (int i = 0; i < FRAMES_IN_FLYIGNT; i++)
    {
        vkDestroyFence(m_vk_device, m_frames[i].m_render_fence, nullptr);
        vkDestroySemaphore(m_vk_device, m_frames[i].m_present_semaphore, nullptr);
        vkDestroySemaphore(m_vk_device, m_frames[i].m_render_semaphore, nullptr);
    }
    return true;
}

bool
render_device::init_descriptors()
{
    m_descriptor_allocator = std::make_unique<vk_utils::descriptor_allocator>();
    m_descriptor_layout_cache = std::make_unique<vk_utils::descriptor_layout_cache>();

    VkDescriptorSetLayoutBinding textureBind = vk_utils::make_descriptor_set_layout_binding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    VkDescriptorSetLayoutCreateInfo set3info = {};
    set3info.bindingCount = 1;
    set3info.flags = 0;
    set3info.pNext = nullptr;
    set3info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set3info.pBindings = &textureBind;

    m_single_texture_set_layout = m_descriptor_layout_cache->create_descriptor_layout(&set3info);

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

vk_device_provider
render_device::get_vk_device_provider()
{
    return []() { return glob::render_device::get()->vk_device(); };
}

vma_allocator_provider
render_device::get_vma_allocator_provider()
{
    return []() { return glob::render_device::get()->allocator(); };
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
render_device::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    // allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo command_buffer_ai =
        vk_utils::make_command_buffer_allocate_info(m_upload_context.m_command_pool, 1);

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(m_vk_device, &command_buffer_ai, &cmd));

    // begin the command buffer recording. We will use this command buffer exactly once, so we
    // want to let vulkan know that
    VkCommandBufferBeginInfo command_buffer_bi =
        vk_utils::make_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &command_buffer_bi));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = vk_utils::make_submit_info(&cmd);

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
                             VkMemoryPropertyFlags required_flags)
{
    // allocate vertex buffer
    VkBufferCreateInfo buffer_ci = {};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.pNext = nullptr;
    buffer_ci.size = alloc_size;

    buffer_ci.usage = usage;

    // let the VMA library know that this data should be writable by CPU, but also readable by
    // GPU
    VmaAllocationCreateInfo vma_alloc_ci = {};
    vma_alloc_ci.usage = memory_usage;
    vma_alloc_ci.requiredFlags = required_flags;

    // allocate the buffer

    return vk_utils::vulkan_buffer::create([this]() { return m_allocator; }, buffer_ci,
                                           vma_alloc_ci);
}
}  // namespace render
}  // namespace agea
