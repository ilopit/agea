#include "vulkan_render/types/vulkan_render_pass_builder.h"

#include "vulkan_render/types/vulkan_render_pass.h"
#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/utils/vulkan_debug.h"
#include "vulkan_render/vulkan_render_device.h"

#include <global_state/global_state.h>

#include <string>

namespace kryga::render
{

render_pass_builder&
render_pass_builder::set_color_format(VkFormat f)
{
    m_color_format = f;
    return *this;
}

render_pass_builder&
render_pass_builder::set_depth_format(VkFormat f)
{
    m_depth_format = f;
    return *this;
}

render_pass_builder&
render_pass_builder::set_color_images(const std::vector<vk_utils::vulkan_image_view_sptr>& ivs,
                                      const std::vector<vk_utils::vulkan_image_sptr>& is)
{
    m_color_images = is;
    m_color_image_views = ivs;
    return *this;
}

render_pass_builder&
render_pass_builder::set_width_depth(uint32_t w, uint32_t h)
{
    m_width = w;
    m_height = h;
    return *this;
}

render_pass_builder&
render_pass_builder::set_enable_stencil(bool stencil)
{
    m_enable_stencil = stencil;
    return *this;
}

render_pass_builder&
render_pass_builder::set_depth_only(bool depth_only)
{
    m_depth_only = depth_only;
    return *this;
}

render_pass_builder&
render_pass_builder::set_image_count(uint32_t count)
{
    m_image_count = count;
    return *this;
}

render_pass_builder&
render_pass_builder::set_debug_name(std::string_view name)
{
    m_debug_name.assign(name);
    return *this;
}

render_pass_sptr
render_pass_builder::build()
{
    KRG_check(m_width, "Should not be 0!");
    KRG_check(m_height, "Should not be 0!");

    auto device = glob::glob_state().getr_render_device().vk_device();

    auto rp = std::make_shared<render_pass>();

    // Create VkRenderPass
    {
        auto attachments = get_attachments();

        VkSubpassDescription subpassDescription = {};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.inputAttachmentCount = 0;
        subpassDescription.pInputAttachments = nullptr;
        subpassDescription.preserveAttachmentCount = 0;
        subpassDescription.pPreserveAttachments = nullptr;
        subpassDescription.pResolveAttachments = nullptr;

        VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthReference = {};

        if (m_depth_only)
        {
            depthReference = {0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            subpassDescription.colorAttachmentCount = 0;
            subpassDescription.pColorAttachments = nullptr;
        }
        else
        {
            depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            subpassDescription.colorAttachmentCount = 1;
            subpassDescription.pColorAttachments = &colorReference;
        }
        subpassDescription.pDepthStencilAttachment = &depthReference;

        auto dependencies = get_dependencies();

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpassDescription;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        vkCreateRenderPass(device, &renderPassInfo, nullptr, &rp->m_vk_render_pass);
        KRG_VK_NAME_FMT(device, rp->m_vk_render_pass, "{}.render_pass", m_debug_name);
    }

    // Determine image count: from color images (normal passes) or explicit count (depth-only)
    uint32_t image_count =
        m_depth_only ? m_image_count : static_cast<uint32_t>(m_color_image_views.size());
    KRG_check(image_count > 0, "Image count must be > 0");

    VkExtent3D depth_image_extent = {m_width, m_height, 1};

    VkImageUsageFlags depth_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (m_depth_only)
    {
        depth_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    auto dimg_info =
        vk_utils::make_image_create_info(m_depth_format, depth_usage, depth_image_extent);

    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (m_depth_only)
    {
        // Depth-only: create depth images without any color images
        rp->m_depth_images.resize(image_count);
        rp->m_depth_image_views.resize(image_count);

        for (uint32_t i = 0; i < image_count; ++i)
        {
            rp->m_depth_images[i] = vk_utils::vulkan_image::create(
                glob::glob_state().getr_render_device().get_vma_allocator_provider(),
                dimg_info,
                dimg_allocinfo,
                0,
                KRG_VK_FMT_NAME("{}.depth_{}", m_debug_name, i));

            auto depth_image_view_ci = vk_utils::make_imageview_create_info(
                m_depth_format, rp->m_depth_images[i].image(), VK_IMAGE_ASPECT_DEPTH_BIT);

            rp->m_depth_image_views[i] = vk_utils::vulkan_image_view::create(depth_image_view_ci);
        }

        // Create framebuffers with depth attachment only
        auto fb_info = vk_utils::make_framebuffer_create_info(rp->m_vk_render_pass,
                                                              VkExtent2D{m_width, m_height});
        rp->m_framebuffers.resize(image_count);

        for (uint32_t i = 0; i < image_count; i++)
        {
            VkImageView fb_attachments[1] = {rp->m_depth_image_views[i].vk()};
            fb_info.pAttachments = fb_attachments;
            fb_info.attachmentCount = 1;
            VK_CHECK(vkCreateFramebuffer(device, &fb_info, nullptr, &rp->m_framebuffers[i]));
        }

        rp->m_color_format = VK_FORMAT_UNDEFINED;
        rp->m_depth_format = m_depth_format;
        rp->m_color_attachment_count = 0;
        rp->m_fixed_width = m_width;
        rp->m_fixed_height = m_height;
    }
    else
    {
        // Normal path: color + depth
        rp->m_depth_image_views.resize(image_count);
        rp->m_depth_images.resize(image_count);
        rp->m_color_images = std::move(m_color_images);
        rp->m_color_image_views = std::move(m_color_image_views);

        for (uint32_t i = 0; i < image_count; ++i)
        {
            rp->m_depth_images[i] = vk_utils::vulkan_image::create(
                glob::glob_state().getr_render_device().get_vma_allocator_provider(),
                dimg_info,
                dimg_allocinfo,
                0,
                KRG_VK_FMT_NAME("{}.depth_{}", m_debug_name, i));

            int depth_image_view_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (m_enable_stencil)
            {
                depth_image_view_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }

            auto depth_image_view_ci = vk_utils::make_imageview_create_info(
                m_depth_format, rp->m_depth_images[i].image(), depth_image_view_flags);

            rp->m_depth_image_views[i] = vk_utils::vulkan_image_view::create(depth_image_view_ci);
        }

        // Create framebuffers
        auto fb_info = vk_utils::make_framebuffer_create_info(rp->m_vk_render_pass,
                                                              VkExtent2D{m_width, m_height});
        rp->m_framebuffers.resize(image_count);

        for (uint32_t i = 0; i < image_count; i++)
        {
            VkImageView fb_attachments[2] = {rp->m_color_image_views[i]->vk(),
                                             rp->m_depth_image_views[i].vk()};

            fb_info.pAttachments = fb_attachments;
            fb_info.attachmentCount = 2;
            VK_CHECK(vkCreateFramebuffer(device, &fb_info, nullptr, &rp->m_framebuffers[i]));
        }

        rp->m_color_format = m_color_format;
        rp->m_depth_format = m_depth_format;
        rp->m_color_attachment_count = 1;
    }

    return rp;
}

std::vector<VkAttachmentDescription>
render_pass_builder::get_attachments()
{
    if (m_depth_only)
    {
        VkAttachmentDescription depth_attachment = {};
        depth_attachment.format = m_depth_format;
        depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        return {depth_attachment};
    }

    std::vector<VkAttachmentDescription> attachments(2);

    // Color attachment. initialLayout=UNDEFINED makes the pass self-sufficient for
    // callers that invoke render_pass::begin directly (e.g. simple regression tests
    // that bypass the render graph). Safe here because loadOp=CLEAR discards any
    // prior contents anyway. The render-graph path still transitions into
    // COLOR_ATTACHMENT_OPTIMAL before begin; re-entering via UNDEFINED here is a
    // harmless no-op discard.
    attachments[0].format = m_color_format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment — internal to this pass, not in render graph.
    // Use UNDEFINED initial layout so VkRenderPass handles the transition.
    attachments[1].format = m_depth_format;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    return attachments;
}

std::vector<VkSubpassDependency>
render_pass_builder::get_dependencies()
{
    // Render graph handles all synchronization via explicit barriers.
    // No subpass external dependencies needed.
    return {};
}

}  // namespace kryga::render
