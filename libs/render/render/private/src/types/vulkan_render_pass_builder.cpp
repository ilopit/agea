#include "vulkan_render/types/vulkan_render_pass_builder.h"

#include "vulkan_render/types/vulkan_render_pass.h"
#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/vulkan_render_device.h"

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
render_pass_builder::set_preset(presets p)
{
    m_preset = p;
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

render_pass_sptr
render_pass_builder::build()
{
    KRG_check(m_width, "Should not be 0!");
    KRG_check(m_height, "Should not be 0!");

    auto device = glob::render_device::getr().vk_device();

    auto rp = std::make_shared<render_pass>();

    // Create VkRenderPass
    {
        auto attachments = get_attachments();

        VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthReference = {1,
                                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpassDescription = {};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &colorReference;
        subpassDescription.pDepthStencilAttachment = &depthReference;
        subpassDescription.inputAttachmentCount = 0;
        subpassDescription.pInputAttachments = nullptr;
        subpassDescription.preserveAttachmentCount = 0;
        subpassDescription.pPreserveAttachments = nullptr;
        subpassDescription.pResolveAttachments = nullptr;

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
    }

    // Setup depth images
    rp->m_depth_image_views.resize(m_color_image_views.size());
    rp->m_depth_images.resize(m_color_image_views.size());
    rp->m_color_images = std::move(m_color_images);
    rp->m_color_image_views = std::move(m_color_image_views);

    VkExtent3D depth_image_extent = {m_width, m_height, 1};

    auto dimg_info = vk_utils::make_image_create_info(
        m_depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depth_image_extent);

    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    for (size_t i = 0; i < rp->m_color_images.size(); ++i)
    {
        rp->m_depth_images[i] = vk_utils::vulkan_image::create(
            glob::render_device::getr().get_vma_allocator_provider(), dimg_info, dimg_allocinfo);

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
    auto fb_info =
        vk_utils::make_framebuffer_create_info(rp->m_vk_render_pass, VkExtent2D{m_width, m_height});

    const uint32_t swapchain_imagecount = static_cast<uint32_t>(rp->m_color_images.size());
    rp->m_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

    for (uint32_t i = 0; i < swapchain_imagecount; i++)
    {
        VkImageView attachments[2] = {rp->m_color_image_views[i]->vk(),
                                      rp->m_depth_image_views[i].vk()};

        fb_info.pAttachments = attachments;
        fb_info.attachmentCount = 2;
        VK_CHECK(vkCreateFramebuffer(device, &fb_info, nullptr, &rp->m_framebuffers[i]));
    }

    // Store format info for shader compatibility validation
    rp->m_color_format = m_color_format;
    rp->m_depth_format = m_depth_format;
    rp->m_color_attachment_count = 1;

    return rp;
}

std::array<VkAttachmentDescription, 2>
render_pass_builder::get_attachments()
{
    std::array<VkAttachmentDescription, 2> attachments = {};

    // Color attachment - common settings
    attachments[0].format = m_color_format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Depth attachment - common settings
    attachments[1].format = m_depth_format;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Preset-specific final layout for color attachment
    switch (m_preset)
    {
    case presets::swapchain:
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        break;
    case presets::buffer:
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        break;
    case presets::picking:
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        break;
    }

    return attachments;
}

std::array<VkSubpassDependency, 2>
render_pass_builder::get_dependencies()
{
    std::array<VkSubpassDependency, 2> dependencies = {};

    // Common structure for all presets
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    switch (m_preset)
    {
    case presets::swapchain:
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[0].dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[1].srcAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        break;

    case presets::buffer:
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        break;

    case presets::picking:
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[1].srcAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        break;
    }

    return dependencies;
}

}  // namespace kryga::render
