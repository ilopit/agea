#include "vulkan_render/types/vulkan_render_pass_builder.h"

#include "vulkan_render/types/vulkan_render_pass.h"
#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/vulkan_render_device.h"

#include <global_state/global_state.h>

#include <unordered_set>

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
                dimg_allocinfo);

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
                dimg_allocinfo);

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

    // Color attachment — render graph handles all layout transitions
    attachments[0].format = m_color_format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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

// ============================================================================
// multi_subpass_render_pass_builder
// ============================================================================

multi_subpass_render_pass_builder::multi_subpass_render_pass_builder(VkDevice device)
    : m_device(device)
{
}

static bool
is_depth_format(VkFormat fmt)
{
    return fmt == VK_FORMAT_D32_SFLOAT || fmt == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           fmt == VK_FORMAT_D16_UNORM || fmt == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkRenderPass
multi_subpass_render_pass_builder::build(const subpass_group_desc& desc)
{
    KRG_check(!desc.subpasses.empty(), "Subpass group must have at least one subpass");

    auto attachments = build_attachments(desc);

    // Storage for attachment references (must outlive VkSubpassDescription)
    std::vector<std::vector<VkAttachmentReference>> color_refs(desc.subpasses.size());
    std::vector<VkAttachmentReference> depth_refs(desc.subpasses.size());
    std::vector<std::vector<VkAttachmentReference>> input_refs(desc.subpasses.size());
    std::vector<std::vector<uint32_t>> preserve_refs(desc.subpasses.size());

    auto subpasses = build_subpasses(desc, color_refs, depth_refs, input_refs, preserve_refs);
    auto dependencies = build_dependencies(desc);

    VkRenderPassCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    create_info.pAttachments = attachments.data();
    create_info.subpassCount = static_cast<uint32_t>(subpasses.size());
    create_info.pSubpasses = subpasses.data();
    create_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
    create_info.pDependencies = dependencies.data();

    VkRenderPass render_pass = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRenderPass(m_device, &create_info, nullptr, &render_pass));

    return render_pass;
}

std::vector<VkClearValue>
multi_subpass_render_pass_builder::get_clear_values(const subpass_group_desc& desc) const
{
    std::vector<VkClearValue> clear_values;
    clear_values.reserve(desc.attachments.size());

    for (const auto& attachment : desc.attachments)
    {
        VkClearValue clear = {};
        if (is_depth_format(attachment.format))
        {
            clear.depthStencil = {1.0f, 0};
        }
        else
        {
            clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        }
        clear_values.push_back(clear);
    }

    return clear_values;
}

std::vector<VkAttachmentDescription>
multi_subpass_render_pass_builder::build_attachments(const subpass_group_desc& desc)
{
    std::vector<VkAttachmentDescription> attachments;
    attachments.reserve(desc.attachments.size());

    for (const auto& att : desc.attachments)
    {
        VkAttachmentDescription vk_att = {};
        vk_att.format = att.format;
        vk_att.samples = VK_SAMPLE_COUNT_1_BIT;
        vk_att.loadOp = att.load_op;
        vk_att.storeOp = att.store_op;

        if (is_depth_format(att.format))
        {
            vk_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            vk_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            vk_att.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            vk_att.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        else
        {
            vk_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            vk_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            vk_att.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            vk_att.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        attachments.push_back(vk_att);
    }

    return attachments;
}

std::vector<VkSubpassDescription>
multi_subpass_render_pass_builder::build_subpasses(
    const subpass_group_desc& desc,
    std::vector<std::vector<VkAttachmentReference>>& color_refs,
    std::vector<VkAttachmentReference>& depth_refs,
    std::vector<std::vector<VkAttachmentReference>>& input_refs,
    std::vector<std::vector<uint32_t>>& preserve_refs)
{
    std::vector<VkSubpassDescription> subpasses;
    subpasses.reserve(desc.subpasses.size());

    for (size_t sp_idx = 0; sp_idx < desc.subpasses.size(); ++sp_idx)
    {
        const auto& sp = desc.subpasses[sp_idx];
        depth_refs[sp_idx] = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};

        // Track which attachments are referenced in this subpass
        std::unordered_set<uint32_t> referenced_attachments;

        for (const auto& ref : sp.attachments)
        {
            referenced_attachments.insert(ref.attachment_index);

            VkAttachmentReference vk_ref = {};
            vk_ref.attachment = ref.attachment_index;

            switch (ref.usage)
            {
            case subpass_attachment_usage::color_output:
                vk_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                color_refs[sp_idx].push_back(vk_ref);
                break;

            case subpass_attachment_usage::depth_output:
                vk_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                depth_refs[sp_idx] = vk_ref;
                break;

            case subpass_attachment_usage::depth_read_only:
                vk_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                depth_refs[sp_idx] = vk_ref;
                break;

            case subpass_attachment_usage::input_attachment:
                vk_ref.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                input_refs[sp_idx].push_back(vk_ref);
                break;

            case subpass_attachment_usage::preserve:
                // Handled below
                break;
            }
        }

        // Build preserve list: attachments not referenced but needed later
        for (const auto& ref : sp.attachments)
        {
            if (ref.usage == subpass_attachment_usage::preserve)
            {
                preserve_refs[sp_idx].push_back(ref.attachment_index);
            }
        }

        VkSubpassDescription vk_sp = {};
        vk_sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        vk_sp.colorAttachmentCount = static_cast<uint32_t>(color_refs[sp_idx].size());
        vk_sp.pColorAttachments = color_refs[sp_idx].empty() ? nullptr : color_refs[sp_idx].data();
        vk_sp.pDepthStencilAttachment =
            (depth_refs[sp_idx].attachment != VK_ATTACHMENT_UNUSED) ? &depth_refs[sp_idx] : nullptr;
        vk_sp.inputAttachmentCount = static_cast<uint32_t>(input_refs[sp_idx].size());
        vk_sp.pInputAttachments = input_refs[sp_idx].empty() ? nullptr : input_refs[sp_idx].data();
        vk_sp.preserveAttachmentCount = static_cast<uint32_t>(preserve_refs[sp_idx].size());
        vk_sp.pPreserveAttachments =
            preserve_refs[sp_idx].empty() ? nullptr : preserve_refs[sp_idx].data();
        vk_sp.pResolveAttachments = nullptr;

        subpasses.push_back(vk_sp);
    }

    return subpasses;
}

std::vector<VkSubpassDependency>
multi_subpass_render_pass_builder::build_dependencies(const subpass_group_desc& desc)
{
    std::vector<VkSubpassDependency> dependencies;

    // External → first subpass dependency
    {
        VkSubpassDependency dep = {};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask = 0;
        dep.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dependencies.push_back(dep);
    }

    // Inter-subpass dependencies for input attachments
    for (size_t dst_sp = 1; dst_sp < desc.subpasses.size(); ++dst_sp)
    {
        const auto& dst_subpass = desc.subpasses[dst_sp];

        for (const auto& ref : dst_subpass.attachments)
        {
            if (ref.usage == subpass_attachment_usage::input_attachment)
            {
                // Find which subpass wrote this attachment
                for (size_t src_sp = 0; src_sp < dst_sp; ++src_sp)
                {
                    const auto& src_subpass = desc.subpasses[src_sp];
                    bool writes_attachment = false;

                    for (const auto& src_ref : src_subpass.attachments)
                    {
                        if (src_ref.attachment_index == ref.attachment_index &&
                            (src_ref.usage == subpass_attachment_usage::color_output ||
                             src_ref.usage == subpass_attachment_usage::depth_output))
                        {
                            writes_attachment = true;
                            break;
                        }
                    }

                    if (writes_attachment)
                    {
                        VkSubpassDependency dep = {};
                        dep.srcSubpass = static_cast<uint32_t>(src_sp);
                        dep.dstSubpass = static_cast<uint32_t>(dst_sp);
                        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                        dep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                        dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                        dep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
                        dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;  // Tile-local
                        dependencies.push_back(dep);
                        break;  // Found the producer
                    }
                }
            }
        }
    }

    return dependencies;
}

}  // namespace kryga::render
