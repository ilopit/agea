#include "vulkan_render/types/vulkan_render_pass.h"

#include <vulkan/vulkan.h>

#include <array>

#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader_create_infos.h"
#include "vulkan_render/vulkan_loaders/vulkan_shader_loader.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/types/vulkan_shader_data.h"

#include <utils/kryga_log.h>

namespace kryga::render
{

render_pass_builder&
render_pass_builder::set_color_images(const std::vector<vk_utils::vulkan_image_view_sptr>& ivs,
                                      const std::vector<vk_utils::vulkan_image_sptr>& is)
{
    m_color_images = is;
    m_color_image_views = ivs;

    return *this;
}

render_pass::render_pass(const utils::id& name, rg_pass_type type)
    : m_name(name)
    , m_type(type)
{
}

render_pass::~render_pass()
{
    if (m_vk_render_pass != VK_NULL_HANDLE)
    {
        glob::render_device::getr().delete_immediately(
            [=](VkDevice vkd, VmaAllocator)
            {
                vkDestroyRenderPass(vkd, m_vk_render_pass, nullptr);

                for (auto f : m_framebuffers)
                {
                    vkDestroyFramebuffer(vkd, f, nullptr);
                }
            });
    }

    m_color_image_views.clear();
    m_color_images.clear();
}

bool
render_pass::begin(VkCommandBuffer cmd,
                   uint64_t swapchain_image_index,
                   uint32_t width,
                   uint32_t height)
{
    if (m_vk_render_pass == VK_NULL_HANDLE || m_framebuffers.empty())
    {
        return false;
    }

    auto fb_idx = swapchain_image_index % m_framebuffers.size();

    auto rp_info = vk_utils::make_renderpass_begin_info(m_vk_render_pass, VkExtent2D{width, height},
                                                        m_framebuffers[fb_idx]);

    VkClearValue clear_values[2];
    clear_values[0].color = m_clear_color;
    clear_values[1].depthStencil = {1.0f, 0};

    rp_info.clearValueCount = 2;
    rp_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

    return true;
}

bool
render_pass::end(VkCommandBuffer cmd)
{
    // finalize the render pass
    vkCmdEndRenderPass(cmd);

    return true;
}

void
render_pass::execute(VkCommandBuffer cmd, uint64_t swapchain_image_index, uint32_t width, uint32_t height)
{
    if (is_graphics())
    {
        // Graphics pass: wrap with begin/end
        begin(cmd, swapchain_image_index, width, height);

        if (m_execute)
        {
            m_execute(cmd);
        }

        end(cmd);
    }
    else
    {
        // Compute or transfer pass: just execute callback
        if (m_execute)
        {
            m_execute(cmd);
        }
    }
}

result_code
render_pass::create_shader_effect(const kryga::utils::id& id,
                                  const shader_effect_create_info& info,
                                  shader_effect_data*& sed)
{
    KRG_check(!get_shader_effect(id), "should never happens");

    auto effect = std::make_shared<shader_effect_data>(id);

    auto info_copy = info;
    info_copy.rp = this;

    auto rc = vulkan_shader_loader::create_shader_effect(*effect, info_copy);

    if (rc != result_code::ok)
    {
        effect->m_failed_load = true;
        effect->set_owner_render_pass(this);
        m_shader_effects[id] = effect;
        sed = effect.get();
        return rc;
    }

    // Validate shader bindings against render pass resources (only if resources are declared)
    if (!m_resources.empty() && effect->m_vertex_stage && effect->m_frag_stage)
    {
        std::string validation_error;
        if (!validate_shader_resources(effect->m_vertex_stage->get_reflection(),
                                       effect->m_frag_stage->get_reflection(), validation_error))
        {
            ALOG_ERROR("Shader effect '{}' resource validation failed: {}",
                       std::string(id.cstr()), validation_error);
            effect->m_failed_load = true;
            effect->set_owner_render_pass(this);
            m_shader_effects[id] = effect;
            sed = effect.get();
            return result_code::validation_error;
        }
    }

    effect->m_failed_load = false;
    effect->set_owner_render_pass(this);

    m_shader_effects[id] = effect;

    sed = effect.get();

    return rc;
}

result_code
render_pass::update_shader_effect(shader_effect_data& se_data, const shader_effect_create_info& info)
{
    KRG_check(get_shader_effect(se_data.get_id()), "should never happens");

    std::shared_ptr<render::shader_effect_data> old_se_data;

    auto info_copy = info;
    info_copy.rp = this;

    auto rc = vulkan_shader_loader::update_shader_effect(se_data, info_copy, old_se_data);

    se_data.m_failed_load = rc != result_code::ok;

    if (rc != result_code::ok)
    {
        ALOG_LAZY_ERROR;
        return rc;
    }

    return rc;
}

shader_effect_data*
render_pass::get_shader_effect(const kryga::utils::id& id)
{
    auto itr = m_shader_effects.find(id);
    return itr != m_shader_effects.end() ? itr->second.get() : nullptr;
}

void
render_pass::destroy_shader_effect(const kryga::utils::id& id)
{
    auto itr = m_shader_effects.find(id);
    if (itr != m_shader_effects.end())
    {
        m_shader_effects.erase(itr);
    }
}

bool
render_pass::validate_fragment_outputs(const reflection::interface_block& frag_outputs,
                                       std::string& out_error) const
{
    // Count fragment shader outputs (only those with valid locations)
    uint32_t output_count = 0;
    for (const auto& var : frag_outputs.variables)
    {
        // Fragment outputs have locations starting from 0
        // Built-ins like gl_FragDepth don't have valid locations in this interface
        output_count++;
    }

    // Validate output count matches color attachment count
    if (output_count != m_color_attachment_count)
    {
        out_error = "Fragment shader has " + std::to_string(output_count) +
                    " output(s), but render pass has " + std::to_string(m_color_attachment_count) +
                    " color attachment(s)";
        return false;
    }

    return true;
}

namespace
{
// Map VkDescriptorType to expected resource type
rg_resource_type
get_expected_resource_type(VkDescriptorType descriptor_type)
{
    switch (descriptor_type)
    {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return rg_resource_type::buffer;

    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return rg_resource_type::image;

    default:
        return rg_resource_type::buffer;
    }
}

// Check if descriptor type requires write access
// Note: Storage buffers can be readonly in GLSL, so we can't assume they need write.
// Only storage images definitely need write access when used as output.
bool
descriptor_requires_write(VkDescriptorType descriptor_type)
{
    switch (descriptor_type)
    {
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return true;
    default:
        // Storage buffers may be readonly - can't determine from descriptor type alone
        return false;
    }
}

// Check if usage provides at least read access
bool
usage_allows_read(rg_resource_usage usage)
{
    return usage == rg_resource_usage::read || usage == rg_resource_usage::read_write;
}

// Check if usage provides write access
bool
usage_allows_write(rg_resource_usage usage)
{
    return usage == rg_resource_usage::write || usage == rg_resource_usage::read_write;
}

const char*
descriptor_type_to_string(VkDescriptorType type)
{
    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return "uniform_buffer";
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return "storage_buffer";
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return "combined_image_sampler";
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return "sampled_image";
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return "storage_image";
    default:
        return "unknown";
    }
}

const char*
resource_type_to_string(rg_resource_type type)
{
    return type == rg_resource_type::buffer ? "buffer" : "image";
}
}  // namespace

bool
render_pass::validate_shader_resources(const reflection::shader_reflection& vertex_reflection,
                                       const reflection::shader_reflection& frag_reflection,
                                       std::string& out_error) const
{
    // Helper to validate a single binding against available resources
    auto validate_binding = [this, &out_error](const reflection::binding& b,
                                               const char* stage_name) -> bool
    {
        // Find resource by name in m_resources
        const rg_resource_ref* found_resource = nullptr;
        for (const auto& res : m_resources)
        {
            if (res.resource == b.name)
            {
                found_resource = &res;
                break;
            }
        }

        if (!found_resource)
        {
            out_error = std::string(stage_name) + " shader binding '" + std::string(b.name.cstr()) +
                        "' (type: " + descriptor_type_to_string(b.type) +
                        ") not found in render pass resources";
            return false;
        }

        // Validate type compatibility
        rg_resource_type expected_type = get_expected_resource_type(b.type);
        if (found_resource->type != expected_type)
        {
            out_error = std::string(stage_name) + " shader binding '" + std::string(b.name.cstr()) +
                        "' expects " + resource_type_to_string(expected_type) +
                        " but render pass resource is " +
                        resource_type_to_string(found_resource->type);
            return false;
        }

        // Validate usage compatibility
        bool needs_write = descriptor_requires_write(b.type);
        if (needs_write && !usage_allows_write(found_resource->usage))
        {
            out_error = std::string(stage_name) + " shader binding '" + std::string(b.name.cstr()) +
                        "' (type: " + descriptor_type_to_string(b.type) +
                        ") requires write access but resource only provides read";
            return false;
        }

        if (!needs_write && !usage_allows_read(found_resource->usage))
        {
            out_error = std::string(stage_name) + " shader binding '" + std::string(b.name.cstr()) +
                        "' (type: " + descriptor_type_to_string(b.type) +
                        ") requires read access but resource only provides write";
            return false;
        }

        return true;
    };

    // Validate all vertex shader bindings
    for (const auto& ds : vertex_reflection.descriptors)
    {
        for (const auto& b : ds.bindings)
        {
            if (!validate_binding(b, "Vertex"))
            {
                return false;
            }
        }
    }

    // Validate all fragment shader bindings
    for (const auto& ds : frag_reflection.descriptors)
    {
        for (const auto& b : ds.bindings)
        {
            if (!validate_binding(b, "Fragment"))
            {
                return false;
            }
        }
    }

    return true;
}

render_pass_sptr
render_pass_builder::build()
{
    KRG_check(m_width, "Should not be 0!");
    KRG_check(m_height, "Should not be 0!");

    auto device = glob::render_device::getr().vk_device();

    auto rp = std::make_shared<render_pass>();

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

        // Subpass dependencies for layout transitions
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

    rp->m_depth_image_views.resize(m_color_image_views.size());
    rp->m_depth_images.resize(m_color_image_views.size());
    rp->m_color_images = std::move(m_color_images);
    rp->m_color_image_views = std::move(m_color_image_views);

    // depth image size will match the window
    VkExtent3D depth_image_extent = {m_width, m_height, 1};

    // the depth image will be a image with the format we selected and Depth Attachment usage
    // flag

    auto dimg_info = vk_utils::make_image_create_info(
        m_depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depth_image_extent);

    // for the depth image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    for (auto i = 0; i < rp->m_color_images.size(); ++i)
    {
        // allocate and create the image
        rp->m_depth_images[i] = vk_utils::vulkan_image::create(
            glob::render_device::getr().get_vma_allocator_provider(), dimg_info, dimg_allocinfo);

        // build a image-view for the depth image to use for rendering

        int depth_image_view_flags = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (m_enable_stencil)
        {
            depth_image_view_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        auto depth_image_view_ci = vk_utils::make_imageview_create_info(
            m_depth_format, rp->m_depth_images[i].image(), depth_image_view_flags);

        rp->m_depth_image_views[i] = vk_utils::vulkan_image_view::create(depth_image_view_ci);
    }

    auto fb_info =
        vk_utils::make_framebuffer_create_info(rp->m_vk_render_pass, VkExtent2D{m_width, m_height});

    const uint32_t swapchain_imagecount = (uint32_t)rp->m_color_images.size();
    rp->m_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

    for (uint32_t i = 0; i < swapchain_imagecount; i++)
    {
        VkImageView attachments[2] = {rp->m_color_image_views[i]->vk(),
                                      rp->m_depth_image_views[i].vk()};

        fb_info.pAttachments = attachments;
        fb_info.attachmentCount = 2;
        VK_CHECK(vkCreateFramebuffer(device, &fb_info, nullptr, &rp->m_framebuffers[i]));
    }

    // Store attachment format info for shader compatibility validation
    rp->m_color_format = m_color_format;
    rp->m_depth_format = m_depth_format;
    rp->m_color_attachment_count = 1;  // Currently we always have 1 color attachment

    return rp;
}

std::array<VkAttachmentDescription, 2>
render_pass_builder::get_attachments()
{
    std::array<VkAttachmentDescription, 2> attachments = {};

    switch (m_preset)
    {
    case presets::swapchain:

        // Color attachment
        attachments[0].format = m_color_format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        // Depth attachment
        attachments[1].format = m_depth_format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        break;

    case presets::buffer:

        attachments[0].format = m_color_format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // Depth attachment
        attachments[1].format = m_depth_format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        break;

    case presets::picking:

        attachments[0].format = m_color_format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        // Depth attachment
        attachments[1].format = m_depth_format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        break;
    }

    return attachments;
}

std::array<VkSubpassDependency, 2>
render_pass_builder::get_dependencies()
{
    std::array<VkSubpassDependency, 2> dependencies;

    switch (m_preset)
    {
    case presets::swapchain:

        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[0].dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[1].srcAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        break;
    case presets::buffer:

        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        break;
    case presets::picking:
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[1].srcAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        break;
    }

    return dependencies;
}
}  // namespace kryga::render