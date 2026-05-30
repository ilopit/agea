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
#include <utils/kryga_log.h>

#include <global_state/global_state.h>
#include <vulkan_render/render_system.h>

#include <VkBootstrap.h>
#include <SDL.h>
#include <SDL_vulkan.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <string>

namespace kryga
{
namespace render
{

namespace
{
VkPresentModeKHR
to_vk_present_mode(present_mode m)
{
    switch (m)
    {
    case present_mode::mailbox:
        return VK_PRESENT_MODE_MAILBOX_KHR;
    case present_mode::immediate:
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    default:
        return VK_PRESENT_MODE_FIFO_KHR;
    }
}

uint64_t
ns_now()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count());
}
}  // namespace

render_device::render_device() = default;

render_device::~render_device() = default;

bool
render_device::construct(construct_params& params)
{
    m_headless = params.headless;
    m_frames_in_flight = std::clamp(params.frames_in_flight, 1u, FRAMES_IN_FLIGHT_MAX);
    m_present_mode = params.present;

    if (params.headless && (params.width < 128 || params.height < 128))
    {
        ALOG_ERROR("headless render_device requires width/height >= 128 (got {}x{})",
                   params.width,
                   params.height);
        return false;
    }

    uint32_t width = params.headless
                         ? params.width
                         : (uint32_t)glob::glob_state().get_native_window()->get_size().w;
    uint32_t height = params.headless
                          ? params.height
                          : (uint32_t)glob::glob_state().get_native_window()->get_size().h;

    init_vulkan(params.window, params.headless);

    // The surface now exists: reconcile the requested (config) present mode +
    // image count against what it supports, falling back if they aren't
    // applicable. Headless has no surface and presents nothing — it keeps the
    // params count as-is. init_swapchain then builds with these resolved values.
    if (!m_headless)
    {
        resolve_present_config(params.present, m_frames_in_flight);
    }

    init_swapchain(params.headless, width, height);

    // Engine-wide invariant: frames_in_flight == swapchain IMAGE count (what the
    // driver actually granted — it may exceed the requested minimum), NOT
    // frame_data count. m_frames is sized to FRAMES_IN_FLIGHT_MAX so it can back
    // any runtime count. Adopt the image count so the frame-slot modulo
    // (switch_frame_indeces) matches the present rotation. Runtime changes go
    // through recreate_swapchain, which preserves this.
    m_frames_in_flight = static_cast<uint32_t>(m_swapchain_images.size());

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

    flush_deferred_deletions();

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

    // deinit_* above may have scheduled more deferred deletions (e.g. swapchain
    // image views) — flush again before destroying the device and allocator.
    flush_deferred_deletions();

    deinit_vulkan();
}

bool
render_device::init_vulkan(SDL_Window* window, bool headless)
{
    vkb::InstanceBuilder builder;

    // Validation layers are not available on stock Android devices without
    // bundling them into the APK (~30MB), so force them off there. Desktop
    // builds keep the debug path honoring KRYGA_VULKAN_DEBUG.
#if defined(__ANDROID__)
    // Android emulators and many real devices only expose Vulkan 1.1; the
    // 1.2-core features we need (descriptor indexing, BDA, scalar block
    // layout) are all available as extensions in 1.1.
    auto inst_ret = builder.set_app_name("KRYGA")
                        .request_validation_layers(false)
                        .set_headless(headless)
                        .require_api_version(1, 1)
                        .build();
#elif KRYGA_VULKAN_DEBUG
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

    // Instance version stays at 1.1 on Android for wider loader compat
    // (some Android 10 loaders don't ship a 1.2 instance), but physical
    // devices must expose 1.2+ so our feature structs (descriptor indexing,
    // BDA, scalar block layout) map to core structs rather than extensions.
    selector.set_required_features(features_11)
        .set_minimum_version(1, 2)
#if defined(__ANDROID__)
        // Android devices are virtually always integrated GPUs (Adreno, Mali,
        // PowerVR). Preferring "discrete" would drop to fallback scoring and
        // may pick an unwanted device.
        .prefer_gpu_device_type(vkb::PreferredDeviceType::integrated);
#else
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete);
#endif

    if (!headless)
    {
        selector.set_surface(m_surface);
    }

#if 0
    // Diagnostic: enumerate GPUs + core-1.2 extension flags at init.
    // Flip to #if 1 when triaging device-specific issues (Adreno/Mali/emulator)
    // without a repro device.
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(m_vk_instance, &count, nullptr);
        std::vector<VkPhysicalDevice> devs(count);
        vkEnumeratePhysicalDevices(m_vk_instance, &count, devs.data());
        for (auto pd : devs)
        {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(pd, &props);
            uint32_t ext_count = 0;
            vkEnumerateDeviceExtensionProperties(pd, nullptr, &ext_count, nullptr);
            std::vector<VkExtensionProperties> exts(ext_count);
            vkEnumerateDeviceExtensionProperties(pd, nullptr, &ext_count, exts.data());
            auto has = [&](const char* name) {
                for (auto& e : exts)
                    if (std::string_view(e.extensionName) == name)
                        return true;
                return false;
            };
            ALOG_INFO("GPU: '{}' api={}.{}.{} indexing={} bda={} scalar={}",
                      props.deviceName,
                      VK_VERSION_MAJOR(props.apiVersion),
                      VK_VERSION_MINOR(props.apiVersion),
                      VK_VERSION_PATCH(props.apiVersion),
                      has(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME),
                      has(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME),
                      has(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME));
        }
    }
#endif
    auto pd_ret = selector.select();
    if (!pd_ret.has_value())
    {
        ALOG_ERROR("vkb::PhysicalDeviceSelector::select failed: {}", pd_ret.error().message());
    }
    vkb::PhysicalDevice physicalDevice = pd_ret.value();
    // create the final vulkan device

    // Optional: VK_KHR_present_wait (+ present_id) for render->display latency.
    // Must run BEFORE the DeviceBuilder below copies physicalDevice's enable-list
    // + feature chain. enable_extension_if_present enables the EXTENSION;
    // enable_extension_features_if_present queries the feature bit AND chains the
    // (internally-copied) feature struct — it does NOT enable the extension, so
    // both are required.
    if (!headless)
    {
        const bool ext_id =
            physicalDevice.enable_extension_if_present(VK_KHR_PRESENT_ID_EXTENSION_NAME);
        const bool ext_wait =
            physicalDevice.enable_extension_if_present(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);

        bool feat_id = false;
        bool feat_wait = false;
        if (ext_id && ext_wait)
        {
            VkPhysicalDevicePresentIdFeaturesKHR pid{};
            pid.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;
            pid.presentId = VK_TRUE;
            VkPhysicalDevicePresentWaitFeaturesKHR pwait{};
            pwait.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;
            pwait.presentWait = VK_TRUE;

            feat_id = physicalDevice.enable_extension_features_if_present(pid);
            feat_wait = physicalDevice.enable_extension_features_if_present(pwait);
        }

        m_present_wait_supported = ext_id && ext_wait && feat_id && feat_wait;
        ALOG_INFO("present_wait detect: ext_id={} ext_wait={} feat_id={} feat_wait={} -> {}",
                  ext_id,
                  ext_wait,
                  feat_id,
                  feat_wait,
                  m_present_wait_supported ? "enabled" : "disabled");
    }

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

    auto dev_ret = deviceBuilder.build();
    if (!dev_ret.has_value())
    {
        ALOG_ERROR("vkb::DeviceBuilder::build failed: {}", dev_ret.error().message());
    }
    vkb::Device vkbDevice = dev_ret.value();

    // Get the VkDevice handle used in the rest of a vulkan application
    m_vk_device = vkbDevice.device;
    m_vk_gpu = physicalDevice.physical_device;

    // Resolve vkWaitForPresentKHR — not exported by the loader .lib, so it must
    // come through vkGetDeviceProcAddr. Disable latency stats if it's missing.
    if (m_present_wait_supported)
    {
        m_vk_wait_for_present = reinterpret_cast<PFN_vkWaitForPresentKHR>(
            vkGetDeviceProcAddr(m_vk_device, "vkWaitForPresentKHR"));
        if (!m_vk_wait_for_present)
        {
            ALOG_WARN("vkGetDeviceProcAddr(vkWaitForPresentKHR) failed; latency stats disabled");
            m_present_wait_supported = false;
        }
    }

    // use vkbootstrap to get a Graphics queue
    m_graphics_queue = vkbDevice.get_queue(vkb::QueueType::graphics).value();

    m_graphics_queue_family = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = m_vk_gpu;
    allocatorInfo.device = m_vk_device;
    allocatorInfo.instance = m_vk_instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
#if defined(__ANDROID__)
    // Android's libvulkan.so only exports Vulkan 1.0 entry points, so VMA is
    // built with VMA_DYNAMIC_VULKAN_FUNCTIONS=1 — it needs these two function
    // pointers to resolve the rest at runtime.
    VmaVulkanFunctions vk_funcs{};
    vk_funcs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vk_funcs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    allocatorInfo.pVulkanFunctions = &vk_funcs;
#endif
    vmaCreateAllocator(&allocatorInfo, &m_allocator);

    vkGetPhysicalDeviceProperties(m_vk_gpu, &m_gpu_properties);
    ALOG_INFO("Selected GPU: '{}' (present_wait {})",
              m_gpu_properties.deviceName,
              m_present_wait_supported ? "enabled" : "disabled");

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
    m_depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;

    if (!headless)
    {
        VkSurfaceCapabilitiesKHR caps{};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vk_gpu, m_surface, &caps));

        // Prefer IDENTITY preTransform when supported; the presentation
        // engine handles any rotation at present time. Mismatched
        // preTransform vs currentTransform yields VK_SUBOPTIMAL_KHR, which
        // we tolerate in vkAcquireNextImageKHR/vkQueuePresentKHR.
        VkSurfaceTransformFlagBitsKHR pre_transform =
            (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
                ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
                : caps.currentTransform;

        uint32_t buffer_w = width;
        uint32_t buffer_h = height;
        // Spec: currentExtent members are set together — sentinel 0xFFFFFFFF
        // means "surface size determined by swapchain extent" (desktop). On a
        // fixed-size surface (Android) caps.currentExtent is authoritative.
        if (caps.currentExtent.width != 0xFFFFFFFFu && caps.currentExtent.height != 0xFFFFFFFFu)
        {
            buffer_w = caps.currentExtent.width;
            buffer_h = caps.currentExtent.height;
        }

        vkb::SwapchainBuilder swapchain_builder{m_vk_gpu, m_vk_device, m_surface};

        VkSurfaceFormatKHR format{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

        // Explicit present mode chosen by the engine (from render_config,
        // platform default otherwise). MAILBOX = low-latency uncapped framerate;
        // FIFO = vsync, lower power/thermal — the sane mobile default.
        const VkPresentModeKHR k_present_mode = to_vk_present_mode(m_present_mode);

#if KRG_EDITOR
        swapchain_builder.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
#endif
        // Request the resolved image count (== frames_in_flight). Without this
        // vkb defaults to caps.minImageCount + 1, which ignores the configured
        // count. The driver may still grant more (minImageCount is a minimum);
        // construct reads back the actual.
        vkb::Swapchain vkb_swapchain = swapchain_builder.set_desired_present_mode(k_present_mode)
                                           .set_desired_extent(buffer_w, buffer_h)
                                           .set_desired_format(format)
                                           .set_pre_transform_flags(pre_transform)
                                           .set_desired_min_image_count(m_frames_in_flight)
                                           .build()
                                           .value();
        // vkb may clamp the requested extent to caps min/max/current —
        // capture the actual swapchain image extent for downstream sizing.
        m_swapchain_extent = vkb_swapchain.extent;

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
        m_swapchain_extent = {width, height};
        VkExtent3D swapchain_image_extent = {width, height, 1};

        auto simg_info = vk_utils::make_image_create_info(
            m_swapchain_image_format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            swapchain_image_extent);

        // for the depth image, we want to allocate it from gpu local memory
        VmaAllocationCreateInfo simg_allocinfo = {};
        simg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        simg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_swapchain_images.resize(m_frames_in_flight);
        m_swapchain_image_views.resize(m_frames_in_flight);
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

    // Size frame_data to the MAX in-flight count, NOT the current swapchain
    // image count. The swapchain image count (== frames_in_flight) varies at
    // runtime in [1, FRAMES_IN_FLIGHT_MAX]; frame_data must already exist for the
    // highest slot the count can reach, or growing the swapchain indexes past the
    // end of m_frames. Headless keeps a fixed count but the spare slots are inert.
    m_frames.resize(FRAMES_IN_FLIGHT_MAX);

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

uint32_t
render_device::recreate_swapchain(
    uint32_t desired_count,
    present_mode mode,
    const std::function<void(const std::vector<vk_utils::vulkan_image_sptr>&,
                             const std::vector<vk_utils::vulkan_image_view_sptr>&)>&
        rebuild_framebuffers)
{
    KRG_check(!m_headless, "recreate_swapchain is windowed-only");

    vkDeviceWaitIdle(m_vk_device);

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vk_gpu, m_surface, &caps);

    VkSurfaceTransformFlagBitsKHR pre_transform =
        (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
            : caps.currentTransform;

    // Present mode is explicit now (the caller's choice), NOT derived from the
    // image count. MAILBOX is non-blocking only with >=3 images, so when it's
    // requested with fewer, bump the image count up to 3. The actual count is
    // read back below and becomes frames_in_flight, so the invariant holds.
    if (mode == present_mode::mailbox && desired_count < 3)
    {
        ALOG_INFO("MAILBOX requested with {} images; forcing image count to 3", desired_count);
        desired_count = 3;
    }
    const VkPresentModeKHR vk_present_mode = to_vk_present_mode(mode);
    m_present_mode = mode;

    // Never request more images than we have frame_data backing — frame slots
    // and swapchain images stay 1:1 (the invariant), and frame_data is fixed at
    // FRAMES_IN_FLIGHT_MAX. m_frames.size() == FRAMES_IN_FLIGHT_MAX.
    desired_count = std::min(desired_count, static_cast<uint32_t>(m_frames.size()));

    VkSurfaceFormatKHR format{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

    vkb::SwapchainBuilder swapchain_builder{m_vk_gpu, m_vk_device, m_surface};
    swapchain_builder.set_desired_present_mode(vk_present_mode)
        .set_desired_extent(m_swapchain_extent.width, m_swapchain_extent.height)
        .set_desired_format(format)
        .set_pre_transform_flags(pre_transform)
        .set_desired_min_image_count(desired_count)
        .set_old_swapchain(m_swapchain);
#if KRG_EDITOR
    swapchain_builder.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
#endif

    auto build_ret = swapchain_builder.build();
    KRG_check(build_ret, "swapchain rebuild failed");
    vkb::Swapchain vkb_swapchain = build_ret.value();

    // Wrap the NEW images/views as locals — the current members stay valid so
    // existing framebuffers keep working until rebuild_framebuffers swaps them.
    std::vector<vk_utils::vulkan_image_sptr> new_images;
    std::vector<vk_utils::vulkan_image_view_sptr> new_views;

    auto images = vkb_swapchain.get_images().value();
    for (size_t idx = 0; idx < images.size(); ++idx)
    {
        new_images.push_back(std::make_shared<vk_utils::vulkan_image>(
            vk_utils::vulkan_image::create(images[idx], vkb_swapchain.image_format)));
    }
    auto views = vkb_swapchain.get_image_views().value();
    for (size_t idx = 0; idx < views.size(); ++idx)
    {
        new_views.push_back(vk_utils::vulkan_image_view::create_shared(std::move(views[idx])));
    }

    // Rebuild pass framebuffers against the NEW views while the OLD ones still
    // exist. This destroys the old framebuffers (releasing their references to
    // the old views), making it safe to drop the old views/swapchain below.
    rebuild_framebuffers(new_images, new_views);

    // Commit: destroy the old swapchain, adopt the new targets. Reassigning the
    // view sptr vector destroys the old VkImageViews — now unreferenced. The old
    // image wrappers are non-owning (the destroyed swapchain owned the VkImages).
    VkSwapchainKHR old_swapchain = m_swapchain;
    m_swapchain = vkb_swapchain.swapchain;
    m_swapchain_image_format = vkb_swapchain.image_format;
    m_swapchain_extent = vkb_swapchain.extent;
    m_swapchain_images = std::move(new_images);
    m_swapchain_image_views = std::move(new_views);

    if (old_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_vk_device, old_swapchain, nullptr);
    }

    uint32_t actual = static_cast<uint32_t>(m_swapchain_images.size());
    // A surface whose minImageCount exceeds our frame_data backing would index
    // past m_frames on the next draw. Fail loudly rather than corrupt memory.
    if (actual > m_frames.size())
    {
        ALOG_ERROR("swapchain granted {} images but only {} frame_data slots exist (max {})",
                   actual,
                   m_frames.size(),
                   FRAMES_IN_FLIGHT_MAX);
    }
    KRG_check(actual <= m_frames.size(), "swapchain image count exceeds frame_data slots");
    m_frames_in_flight = actual;

    // Rebuild per-image present sync to the new image count. Device is idle
    // (waited at the top), so destroying the old semaphores is safe.
    for (auto sem : m_image_render_semaphores)
    {
        vkDestroySemaphore(m_vk_device, sem, nullptr);
    }
    m_image_render_semaphores.clear();
    VkSemaphoreCreateInfo sem_ci = vk_utils::make_semaphore_create_info();
    m_image_render_semaphores.resize(actual);
    for (uint32_t i = 0; i < actual; ++i)
    {
        VK_CHECK(vkCreateSemaphore(m_vk_device, &sem_ci, nullptr, &m_image_render_semaphores[i]));
        KRG_VK_NAME_FMT(m_vk_device, m_image_render_semaphores[i], "image_{}.render_sem", i);
    }
    m_images_in_flight.assign(actual, VK_NULL_HANDLE);

    // Present ids are scoped to a swapchain; the old ids are meaningless against
    // the new one. Restart the counter and drop in-flight latency samples.
    m_present_pending.clear();
    m_present_id = 0;

    ALOG_INFO("recreate_swapchain: desired {} -> {} images (present_mode {})",
              desired_count,
              actual,
              to_string(mode));
    return actual;
}

render_device::image_count_range
render_device::present_mode_image_range(present_mode mode) const
{
    KRG_check(!m_headless, "present_mode_image_range is windowed-only");

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vk_gpu, m_surface, &caps);

    uint32_t lo = std::max(caps.minImageCount, 1u);
    // Mailbox is non-blocking only with >=3 images (1 shown + 1 queued + 1 drawn).
    if (mode == present_mode::mailbox)
    {
        lo = std::max(lo, 3u);
    }

    // maxImageCount == 0 means "no driver limit"; either way cap to the frame_data
    // backing so the UI can't request a count we have no frame slots for.
    uint32_t hi = (caps.maxImageCount == 0) ? FRAMES_IN_FLIGHT_MAX
                                            : std::min(caps.maxImageCount, FRAMES_IN_FLIGHT_MAX);
    hi = std::max(hi, lo);
    return {lo, hi};
}

bool
render_device::is_present_mode_supported(present_mode mode) const
{
    KRG_check(!m_headless, "is_present_mode_supported is windowed-only");

    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_vk_gpu, m_surface, &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_vk_gpu, m_surface, &count, modes.data());

    const VkPresentModeKHR want = to_vk_present_mode(mode);
    for (auto m : modes)
    {
        if (m == want)
        {
            return true;
        }
    }
    return false;
}

void
render_device::resolve_present_config(present_mode requested_mode, uint32_t requested_count)
{
    present_mode mode = requested_mode;
    if (!is_present_mode_supported(mode))
    {
        // Prefer mailbox (lowest latency); fall back to fifo, which the spec
        // guarantees every surface supports.
        present_mode fallback = is_present_mode_supported(present_mode::mailbox)
                                    ? present_mode::mailbox
                                    : present_mode::fifo;
        ALOG_WARN("present mode '{}' not supported by surface; falling back to '{}'",
                  to_string(mode),
                  to_string(fallback));
        mode = fallback;
    }
    m_present_mode = mode;

    const image_count_range range = present_mode_image_range(mode);
    const uint32_t count = std::clamp(requested_count, range.min, range.max);
    if (count != requested_count)
    {
        ALOG_WARN("frames_in_flight {} out of range [{}, {}] for present '{}'; using {}",
                  requested_count,
                  range.min,
                  range.max,
                  to_string(mode),
                  count);
    }
    m_frames_in_flight = count;
}

const uint64_t*
render_device::begin_present_timing()
{
    if (!m_present_wait_supported)
    {
        return nullptr;
    }
    m_current_present_id = ++m_present_id;
    m_present_pending.push_back({m_current_present_id, ns_now()});
    // Safety cap — the deque should never hold more than a few frames in flight.
    while (m_present_pending.size() > 64)
    {
        m_present_pending.pop_front();
    }
    return &m_current_present_id;
}

void
render_device::poll_present_timing()
{
    if (!m_present_wait_supported)
    {
        return;
    }
    const uint64_t now = ns_now();
    while (!m_present_pending.empty())
    {
        const present_stamp& front = m_present_pending.front();
        // timeout 0 -> non-blocking check. Present ids complete in submission
        // order, so once the oldest isn't displayed yet, none after it are.
        VkResult r = m_vk_wait_for_present(m_vk_device, m_swapchain, front.id, 0);
        if (r != VK_SUCCESS)
        {
            break;
        }
        const float ms = static_cast<float>(now - front.submit_ns) * 1e-6f;
        m_present_latency_ms =
            (m_present_latency_ms <= 0.0f) ? ms : 0.9f * m_present_latency_ms + 0.1f * ms;
        m_present_pending.pop_front();
    }
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

    // Per-image present sync (windowed only — headless has no swapchain images
    // and presents nothing).
    m_image_render_semaphores.resize(m_swapchain_images.size());
    for (size_t i = 0; i < m_image_render_semaphores.size(); ++i)
    {
        VK_CHECK(vkCreateSemaphore(
            m_vk_device, &semaphoreCreateInfo, nullptr, &m_image_render_semaphores[i]));
        KRG_VK_NAME_FMT(m_vk_device, m_image_render_semaphores[i], "image_{}.render_sem", i);
    }
    m_images_in_flight.assign(m_swapchain_images.size(), VK_NULL_HANDLE);

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

    for (auto sem : m_image_render_semaphores)
    {
        vkDestroySemaphore(m_vk_device, sem, nullptr);
    }
    m_image_render_semaphores.clear();
    m_images_in_flight.clear();  // non-owning
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
    return []() { return glob::glob_state().getr_render().device.vk_device(); };
}

vma_allocator_provider
render_device::get_vma_allocator_provider()
{
    return []() { return glob::glob_state().getr_render().device.allocator(); };
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
    m_delayed_delete_queue.push({m_current_frame_number + m_frames_in_flight, std::move(d)});
}

void
render_device::delete_immediately(delayed_deleter d)
{
    d(m_vk_device, m_allocator);
}

void
render_device::delete_scheduled_actions()
{
    auto current_frame = get_current_frame_number();
    while (!m_delayed_delete_queue.empty() &&
           m_delayed_delete_queue.top().frame_idx <= current_frame)
    {
        auto del = m_delayed_delete_queue.top().del;
        m_delayed_delete_queue.pop();
        del(m_vk_device, m_allocator);
    }
}

void
render_device::flush_deferred_deletions()
{
    while (!m_delayed_delete_queue.empty())
    {
        auto del = m_delayed_delete_queue.top().del;
        m_delayed_delete_queue.pop();
        del(m_vk_device, m_allocator);
    }
}

render_device::memory_stats
render_device::get_memory_stats() const
{
    memory_stats stats{};
    if (!m_allocator)
    {
        return stats;
    }

    VmaTotalStatistics vma_stats{};
    vmaCalculateStatistics(m_allocator, &vma_stats);

    stats.allocation_count = vma_stats.total.statistics.allocationCount;

    VkPhysicalDeviceMemoryProperties mem_props{};
    vkGetPhysicalDeviceMemoryProperties(m_vk_gpu, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryHeapCount; ++i)
    {
        auto& heap = mem_props.memoryHeaps[i];
        auto used = vma_stats.memoryHeap[i].statistics.allocationBytes;

        if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        {
            stats.device_total += heap.size;
            stats.device_used += used;
        }
        else
        {
            stats.host_total += heap.size;
            stats.host_used += used;
        }
    }
    return stats;
}

void
render_device::log_memory_stats() const
{
    auto s = get_memory_stats();
    auto mb = [](uint64_t b) { return static_cast<double>(b) / (1024.0 * 1024.0); };

    ALOG_INFO("VMA memory: device {:.1f}/{:.1f} MB, host {:.1f}/{:.1f} MB, {} allocations",
              mb(s.device_used),
              mb(s.device_total),
              mb(s.host_used),
              mb(s.host_total),
              s.allocation_count);
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
