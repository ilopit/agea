#include "vulkan_render/render_device.h"

#include "VkBootstrap.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "core/vk_initializers.h"
#include "core/vk_descriptors.h "
#include "core/native_window.h"
#include "core/vk_engine.h"
#include "utils/process.h"

#include "VkBootstrap.h"

#include <iostream>
#include <fstream>

#include "utils/file_utils.h"

#include "vulkan_render/vulkan_texture_data.h"
#include "vulkan_render/vulkan_shader_data.h"
#include "vulkan_render/vulkan_shader_effect.h"
#include "vulkan_render/vulkan_mesh_data.h"
#include "vulkan_render/render_loader.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl.h"
#include "backends/imgui_impl_vulkan.h"

namespace agea
{

namespace render
{

render_device::render_device() = default;

render_device::~render_device() = default;

bool
render_device::construct(construct_params& params)
{
    m_headless = params.headless;

    init_vulkan(params.window);

    init_swapchain();

    init_samplers();

    init_default_renderpass();

    init_framebuffers();

    init_commands();

    init_sync_structures();

    init_descriptors();

    init_pipelines();

    init_imgui();

    return true;
}

void
render_device::destruct()
{
    for (auto& frame : m_frames)
    {
        frame.m_dynamic_descriptor_allocator->cleanup();
    }

    for (auto l : m_todo_layouts)
    {
        vkDestroyDescriptorSetLayout(m_vk_device, l, nullptr);
    }
    m_descriptor_allocator->cleanup();
    m_descriptor_layout_cache->cleanup();

    deinit_imgui();

    deinit_pipelines();

    deinit_sync_structures();

    deinit_commands();

    deinit_framebuffers();

    deinit_default_renderpass();

    deinit_samplers();

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
                        .set_headless(m_headless)
                        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    // grab the instance
    m_vk_instance = vkb_inst.instance;
    m_debug_msg = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(window, m_vk_instance, &m_surface);

    // use vkbootstrap to select a gpu.
    // We want a gpu that can write to the SDL surface and supports vulkan 1.2
    vkb::PhysicalDeviceSelector selector{vkb_inst};

    selector.set_minimum_version(1, 2).prefer_gpu_device_type(vkb::PreferredDeviceType::discrete);
    if (!m_headless)
    {
        selector.set_surface(m_surface);
    }

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

    if (m_headless)
    {
        return true;
    }

    vkb::SwapchainBuilder swapchainBuilder{m_vk_gpu, m_vk_device, m_surface};

    auto width = (uint32_t)glob::native_window::get()->get_size().w;
    auto height = (uint32_t)glob::native_window::get()->get_size().h;

    VkSurfaceFormatKHR format{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

    vkb::Swapchain vkbSwapchain = swapchainBuilder
                                      .use_default_format_selection()
                                      // use vsync present mode
                                      .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
                                      .set_desired_extent(width, height)
                                      .set_desired_format(format)
                                      .build()
                                      .value();

    // store swapchain and its related images
    m_swapchain = vkbSwapchain.swapchain;
    m_swapchain_images = vkbSwapchain.get_images().value();
    m_swapchain_image_views = vkbSwapchain.get_image_views().value();

    m_swachain_image_format = vkbSwapchain.image_format;

    // depth image size will match the window
    VkExtent3D depthImageExtent = {width, height, 1};

    // the depth image will be a image with the format we selected and Depth Attachment usage flag
    VkImageCreateInfo dimg_info = vk_init::image_create_info(
        m_depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

    // for the depth image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    vmaCreateImage(m_allocator, &dimg_info, &dimg_allocinfo, &m_depth_image.m_image,
                   &m_depth_image.m_allocation, nullptr);

    // build a image-view for the depth image to use for rendering
    VkImageViewCreateInfo dview_info = vk_init::imageview_create_info(
        m_depth_format, m_depth_image.m_image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(m_vk_device, &dview_info, nullptr, &m_depth_image_view));

    return true;
}

bool
render_device::deinit_swapchain()
{
    if (m_headless)
    {
        return true;
    }

    m_depth_image.clear();

    vkDestroyImageView(m_vk_device, m_depth_image_view, nullptr);

    vkDestroySwapchainKHR(m_vk_device, m_swapchain, nullptr);

    for (auto i : m_swapchain_image_views)
    {
        vkDestroyImageView(m_vk_device, i, nullptr);
    }

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
    if (m_headless)
    {
        return true;
    }
    // create the framebuffers for the swapchain images. This will connect the render-pass to the
    // images for rendering
    m_frames.resize(FRAMES_IN_FLYIGNT);

    auto width = (uint32_t)glob::native_window::get()->get_size().w;
    auto height = (uint32_t)glob::native_window::get()->get_size().h;

    VkFramebufferCreateInfo fb_info =
        vk_init::framebuffer_create_info(m_render_pass, VkExtent2D{width, height});

    const uint32_t swapchain_imagecount = (uint32_t)m_swapchain_images.size();
    m_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

    for (uint32_t i = 0; i < swapchain_imagecount; i++)
    {
        VkImageView attachments[2];
        attachments[0] = m_swapchain_image_views[i];
        attachments[1] = m_depth_image_view;

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
    VkCommandPoolCreateInfo commandPoolInfo = vk_init::command_pool_create_info(
        m_graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (auto& frame : m_frames)
    {
        VK_CHECK(
            vkCreateCommandPool(m_vk_device, &commandPoolInfo, nullptr, &frame.m_command_pool));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo =
            vk_init::command_buffer_allocate_info(frame.m_command_pool, 1);

        VK_CHECK(
            vkAllocateCommandBuffers(m_vk_device, &cmdAllocInfo, &frame.m_main_command_buffer));
    }

    VkCommandPoolCreateInfo uploadCommandPoolInfo =
        vk_init::command_pool_create_info(m_graphics_queue_family);
    // create pool for upload context
    VK_CHECK(vkCreateCommandPool(m_vk_device, &uploadCommandPoolInfo, nullptr,
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
    VkFenceCreateInfo fenceCreateInfo = vk_init::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    VkSemaphoreCreateInfo semaphoreCreateInfo = vk_init::semaphore_create_info();

    for (auto& frame : m_frames)
    {
        VK_CHECK(vkCreateFence(m_vk_device, &fenceCreateInfo, nullptr, &frame.m_render_fence));

        VK_CHECK(vkCreateSemaphore(m_vk_device, &semaphoreCreateInfo, nullptr,
                                   &frame.m_present_semaphore));
        VK_CHECK(vkCreateSemaphore(m_vk_device, &semaphoreCreateInfo, nullptr,
                                   &frame.m_render_semaphore));
    }

    VkFenceCreateInfo uploadFenceCreateInfo = vk_init::fence_create_info();

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
render_device::init_pipelines()
{
    if (m_headless)
    {
        return true;
    }

    create_textured_pipeline();

    return true;
}

bool
render_device::deinit_pipelines()
{
    for (auto& p : m_pipelines)
    {
        vkDestroyPipeline(m_vk_device, p.second.p, nullptr);
        vkDestroyPipelineLayout(m_vk_device, p.second.l, nullptr);
    }

    return true;
}

bool
render_device::init_imgui()
{
    if (m_headless)
    {
        return true;
    }
    // 1: create descriptor pool for IMGUI
    // the size of the pool is very oversize, but its copied from imgui demo itself.
    VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VK_CHECK(vkCreateDescriptorPool(m_vk_device, &pool_info, nullptr, &m_imguiPool));

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    // this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(glob::native_window::get()->handle());

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_vk_instance;
    init_info.PhysicalDevice = m_vk_gpu;
    init_info.Device = m_vk_device;
    init_info.Queue = m_graphics_queue;
    init_info.DescriptorPool = m_imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    ImGuiIO& io = ImGui::GetIO();

    std::string path =
        glob::resource_locator::get()->resource(category::fonts, "Roboto-Medium.ttf");

    io.Fonts->AddFontFromFileTTF(path.c_str(), 26.0f);

    ImGui_ImplVulkan_Init(&init_info, m_render_pass);

    // execute a gpu command to upload imgui font textures
    immediate_submit([&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });

    // clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    return true;
}

bool
render_device::init_descriptors()
{
    m_descriptor_allocator = std::make_unique<vk_utils::descriptor_allocator>();
    m_descriptor_allocator->init(m_vk_device);

    m_descriptor_layout_cache = std::make_unique<vk_utils::descriptor_layout_cache>();
    m_descriptor_layout_cache->init(m_vk_device);

    VkDescriptorSetLayoutBinding textureBind = vk_init::descriptorset_layout_binding(
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

        frame.m_dynamic_descriptor_allocator->init(m_vk_device);

        const int MAX_OBJECTS = 10000;
        frame.m_object_buffer =
            create_buffer(sizeof(gpu_object_data) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                          VMA_MEMORY_USAGE_CPU_TO_GPU);

        // 10 megabyte of dynamic data buffer
        frame.m_dynamic_data_buffer = create_buffer(10'000'000, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                    VMA_MEMORY_USAGE_CPU_TO_GPU);
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

bool
render_device::create_textured_pipeline()
{
    auto mesh_module = glob::render_loader::get()->load_shader("pbr.vert.spv");
    if (!mesh_module)
    {
        std::cout << "Error when building the colored mesh shader" << std::endl;
        return false;
    }

    auto texture_module = glob::render_loader::get()->load_shader("pbr.frag.spv");
    if (!texture_module)
    {
        std::cout << "Error when building the colored mesh shader" << std::endl;
        return false;
    }

    shader_effect::reflection_overrides overrides[] = {
        {"sceneData", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC},
        {"cameraData", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC}};

    // build the stage-create-info for both vertex and fragment stages. This lets the pipeline
    // know the shader modules per stage
    pipeline_builder pb;

    m_effects.emplace_back(std::make_unique<shader_effect>());
    auto textured_effect = m_effects.back().get();

    textured_effect->add_stage(mesh_module, VK_SHADER_STAGE_VERTEX_BIT);
    textured_effect->add_stage(texture_module, VK_SHADER_STAGE_FRAGMENT_BIT);
    textured_effect->reflect_layout(this, overrides, 2);

    m_todo_layouts.insert(m_todo_layouts.end(), textured_effect->m_set_layouts.begin(),
                          textured_effect->m_set_layouts.end());

    auto texture_pipeline_layout = textured_effect->m_build_layout;

    // vertex input controls how to read vertices from vertex buffers. We arent using it yet
    pb.m_vertex_input_info = vk_init::vertex_input_state_create_info();

    // input assembly is the configuration for drawing triangle lists, strips, or individual
    // points. we are just going to draw triangle list
    pb.m_input_assembly = vk_init::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    // build viewport and scissor from the swapchain extents
    auto width = (uint32_t)glob::native_window::get()->get_size().w;
    auto height = (uint32_t)glob::native_window::get()->get_size().h;
    pb.m_viewport.x = 0.0f;
    pb.m_viewport.y = 0.0f;
    pb.m_viewport.width = (float)width;
    pb.m_viewport.height = (float)height;
    pb.m_viewport.minDepth = 0.0f;
    pb.m_viewport.maxDepth = 1.0f;

    pb.m_scissor.offset = {0, 0};
    pb.m_scissor.extent = VkExtent2D{width, height};

    // configure the rasterizer to draw filled triangles
    pb.m_rasterizer = vk_init::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

    // we dont use multisampling, so just run the default one
    pb.m_multisampling = vk_init::multisampling_state_create_info();

    // a single blend attachment with no blending and writing to RGBA
    pb.m_color_blend_attachment = vk_init::color_blend_attachment_state();

    // default depth testing
    pb.m_depth_stencil =
        vk_init::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

    // build the mesh pipeline
    auto vertexDescription = render::vertex_data::get_vertex_description();

    // connect the pipeline builder vertex input info to the one we get from Vertex
    pb.m_vertex_input_info.pVertexAttributeDescriptions = vertexDescription.attributes.data();
    pb.m_vertex_input_info.vertexAttributeDescriptionCount =
        (uint32_t)vertexDescription.attributes.size();

    pb.m_vertex_input_info.pVertexBindingDescriptions = vertexDescription.bindings.data();
    pb.m_vertex_input_info.vertexBindingDescriptionCount =
        (uint32_t)vertexDescription.bindings.size();

    auto meshVertShader = mesh_module->vk_module();
    pb.m_shader_stages.push_back(
        vk_init::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));

    auto texturedMeshShader = texture_module->vk_module();
    pb.m_shader_stages.push_back(vk_init::pipeline_shader_stage_create_info(
        VK_SHADER_STAGE_FRAGMENT_BIT, texturedMeshShader));

    pb.m_pipelineLayout = texture_pipeline_layout;

    auto texture_pipeline = pb.build_pipeline(m_vk_device, m_render_pass);
    m_pipelines["textured"] = {texture_pipeline, texture_pipeline_layout};

    glob::render_loader::get()->create_default_material(texture_pipeline, textured_effect,
                                                        "simple_texture");
    return true;
}

bool
render_device::deinit_imgui()
{
    vkDestroyDescriptorPool(m_vk_device, m_imguiPool, nullptr);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    return true;
}

void
render_device::init_samplers()
{
    VkSamplerCreateInfo samplerInfo = vk_init::sampler_create_info(VK_FILTER_LINEAR);

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    // info.anisotropyEnable = true;
    // samplerInfo.mipLodBias = 2;
    samplerInfo.maxLod = 30.f;
    // samplerInfo.minLod = 3;
    VkSampler sampler = VK_NULL_HANDLE;
    vkCreateSampler(glob::render_device::get()->vk_device(), &samplerInfo, nullptr, &sampler);

    m_samplers["default"] = sampler;
}

void
render_device::deinit_samplers()
{
    for (auto& s : m_samplers)
    {
        vkDestroySampler(vk_device(), s.second, nullptr);
    }
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
    VkCommandBuffer cmd;

    // allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo =
        vk_init::command_buffer_allocate_info(m_upload_context.m_command_pool, 1);

    VK_CHECK(vkAllocateCommandBuffers(m_vk_device, &cmdAllocInfo, &cmd));

    // begin the command buffer recording. We will use this command buffer exactly once, so we
    // want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo =
        vk_init::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = vk_init::submit_info(&cmd);

    // submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(m_graphics_queue, 1, &submit, m_upload_context.m_upload_fence));

    vkWaitForFences(m_vk_device, 1, &m_upload_context.m_upload_fence, true, 9999999999);
    vkResetFences(m_vk_device, 1, &m_upload_context.m_upload_fence);

    vkResetCommandPool(m_vk_device, m_upload_context.m_command_pool, 0);
}

uint32_t
render_device::pad_uniform_buffer_size(uint32_t originalSize)
{
    // Calculate required alignment based on minimum device offset alignment
    uint32_t minUboAlignment = (uint32_t)m_gpu_properties.limits.minUniformBufferOffsetAlignment;
    uint32_t alignedSize = originalSize;
    if (minUboAlignment > 0)
    {
        alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }
    return alignedSize;
}

allocated_buffer
render_device::create_buffer(size_t allocSize,
                             VkBufferUsageFlags usage,
                             VmaMemoryUsage memoryUsage,
                             VkMemoryPropertyFlags required_flags)
{
    // allocate vertex buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;

    bufferInfo.usage = usage;

    // let the VMA library know that this data should be writable by CPU, but also readable by
    // GPU
    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.requiredFlags = required_flags;

    // allocate the buffer

    return allocated_buffer::create(bufferInfo, vmaallocInfo);
}
}  // namespace render
}  // namespace agea
