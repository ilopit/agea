#include "vulkan_render/utils/vulkan_initializers.h"

namespace agea::render::vk_utils
{

VkCommandPoolCreateInfo
make_command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolResetFlags flags /*= 0*/)
{
    VkCommandPoolCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.pNext = nullptr;
    ci.queueFamilyIndex = queueFamilyIndex;
    ci.flags = flags;

    return ci;
}

VkCommandBufferAllocateInfo
make_command_buffer_allocate_info(VkCommandPool pool,
                                  uint32_t count /*= 1*/,
                                  VkCommandBufferLevel level /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/)
{
    VkCommandBufferAllocateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ci.pNext = nullptr;
    ci.commandPool = pool;
    ci.commandBufferCount = count;
    ci.level = level;

    return ci;
}

VkCommandBufferBeginInfo
make_command_buffer_begin_info(VkCommandBufferUsageFlags flags /*= 0*/)
{
    VkCommandBufferBeginInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    ci.pNext = nullptr;
    ci.pInheritanceInfo = nullptr;
    ci.flags = flags;

    return ci;
}

VkFramebufferCreateInfo
make_framebuffer_create_info(VkRenderPass render_pass, VkExtent2D extent)
{
    VkFramebufferCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.pNext = nullptr;
    ci.renderPass = render_pass;
    ci.attachmentCount = 1;
    ci.width = extent.width;
    ci.height = extent.height;
    ci.layers = 1;

    return ci;
}

VkFenceCreateInfo
make_fence_create_info(VkFenceCreateFlags flags /*= 0*/)
{
    VkFenceCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = flags;

    return ci;
}

VkSemaphoreCreateInfo
make_semaphore_create_info(VkSemaphoreCreateFlags flags /*= 0*/)
{
    VkSemaphoreCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = flags;

    return ci;
}

VkSubmitInfo
make_submit_info(VkCommandBuffer* cmd)
{
    VkSubmitInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    ci.pNext = nullptr;
    ci.waitSemaphoreCount = 0;
    ci.pWaitSemaphores = nullptr;
    ci.pWaitDstStageMask = nullptr;
    ci.commandBufferCount = 1;
    ci.pCommandBuffers = cmd;
    ci.signalSemaphoreCount = 0;
    ci.pSignalSemaphores = nullptr;

    return ci;
}

VkPresentInfoKHR
make_present_info()
{
    VkPresentInfoKHR ci = {};
    ci.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    ci.pNext = nullptr;
    ci.swapchainCount = 0;
    ci.pSwapchains = nullptr;
    ci.pWaitSemaphores = nullptr;
    ci.waitSemaphoreCount = 0;
    ci.pImageIndices = nullptr;

    return ci;
}

VkRenderPassBeginInfo
make_renderpass_begin_info(VkRenderPass render_pass, VkExtent2D extent, VkFramebuffer frame_buffer)
{
    VkRenderPassBeginInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    ci.pNext = nullptr;
    ci.renderPass = render_pass;
    ci.renderArea.offset.x = 0;
    ci.renderArea.offset.y = 0;
    ci.renderArea.extent = extent;
    ci.clearValueCount = 1;
    ci.pClearValues = nullptr;
    ci.framebuffer = frame_buffer;

    return ci;
}

VkPipelineShaderStageCreateInfo
make_pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shader_module)
{
    VkPipelineShaderStageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ci.pNext = nullptr;
    ci.stage = stage;
    ci.module = shader_module;
    ci.pName = "main";

    return ci;
}
VkPipelineVertexInputStateCreateInfo
make_vertex_input_state_create_info()
{
    VkPipelineVertexInputStateCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    ci.pNext = nullptr;
    ci.vertexBindingDescriptionCount = 0;
    ci.vertexAttributeDescriptionCount = 0;

    return ci;
}

VkPipelineInputAssemblyStateCreateInfo
make_input_assembly_create_info(VkPrimitiveTopology topology)
{
    VkPipelineInputAssemblyStateCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ci.pNext = nullptr;
    ci.topology = topology;
    ci.primitiveRestartEnable = VK_FALSE;

    return ci;
}
VkPipelineRasterizationStateCreateInfo
make_rasterization_state_create_info(VkPolygonMode polygon_mode, VkCullModeFlags cull_mode)
{
    VkPipelineRasterizationStateCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    ci.pNext = nullptr;
    ci.depthClampEnable = VK_FALSE;
    ci.rasterizerDiscardEnable = VK_FALSE;
    ci.polygonMode = polygon_mode;
    ci.lineWidth = 1.0f;
    ci.cullMode = cull_mode;
    ci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    ci.depthBiasEnable = VK_FALSE;
    ci.depthBiasConstantFactor = 0.0f;
    ci.depthBiasClamp = 0.0f;
    ci.depthBiasSlopeFactor = 0.0f;

    return ci;
}
VkPipelineMultisampleStateCreateInfo
make_multisampling_state_create_info()
{
    VkPipelineMultisampleStateCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ci.pNext = nullptr;
    ci.sampleShadingEnable = VK_FALSE;
    ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    ci.minSampleShading = 1.0f;
    ci.pSampleMask = nullptr;
    ci.alphaToCoverageEnable = VK_FALSE;
    ci.alphaToOneEnable = VK_FALSE;

    return ci;
}

VkPipelineColorBlendAttachmentState
make_color_blend_attachment_state(alpha_mode enable_alpha)
{
    VkPipelineColorBlendAttachmentState state = {};

    switch (enable_alpha)
    {
    case alpha_mode::none:
    {
        state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        state.blendEnable = VK_FALSE;
        break;
    }
    case alpha_mode::world:
    {
        state.blendEnable = VK_TRUE;

        state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

        state.alphaBlendOp = VK_BLEND_OP_ADD;
        state.colorBlendOp = VK_BLEND_OP_ADD;

        state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        break;
    }
    case alpha_mode::ui:
    {
        state.blendEnable = VK_TRUE;

        state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

        state.alphaBlendOp = VK_BLEND_OP_ADD;
        state.colorBlendOp = VK_BLEND_OP_ADD;

        state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;
    }
    }

    return state;
}

VkPipelineLayoutCreateInfo
make_pipeline_layout_create_info()
{
    VkPipelineLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.setLayoutCount = 0;
    ci.pSetLayouts = nullptr;
    ci.pushConstantRangeCount = 0;
    ci.pPushConstantRanges = nullptr;

    return ci;
}

VkImageCreateInfo
make_image_create_info(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent)
{
    VkImageCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.pNext = nullptr;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = format;
    ci.extent = extent;
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage_flags;

    return ci;
}

VkImageViewCreateInfo
make_imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags)
{
    VkImageViewCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.pNext = nullptr;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.image = image;
    ci.format = format;
    ci.subresourceRange.baseMipLevel = 0;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.baseArrayLayer = 0;
    ci.subresourceRange.layerCount = 1;
    ci.subresourceRange.aspectMask = aspect_flags;

    return ci;
}

VkPipelineDepthStencilStateCreateInfo
make_depth_stencil_create_info(bool depth_test,
                               bool depth_write,
                               VkCompareOp depth_compare_op,
                               depth_stencil_mode enable_stencil)
{
    VkPipelineDepthStencilStateCreateInfo cci = {};
    cci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    cci.pNext = nullptr;
    cci.depthTestEnable = depth_test ? VK_TRUE : VK_FALSE;
    cci.depthWriteEnable = depth_write ? VK_TRUE : VK_FALSE;
    cci.depthCompareOp = depth_compare_op;
    cci.depthBoundsTestEnable = VK_FALSE;

    if (enable_stencil == depth_stencil_mode::stencil)
    {
        cci.stencilTestEnable = VK_TRUE;
        cci.back.compareOp = VK_COMPARE_OP_ALWAYS;
        cci.back.failOp = VK_STENCIL_OP_REPLACE;
        cci.back.depthFailOp = VK_STENCIL_OP_REPLACE;
        cci.back.passOp = VK_STENCIL_OP_REPLACE;
        cci.back.compareMask = 0xff;
        cci.back.writeMask = 0xff;
        cci.back.reference = 1;
        cci.front = cci.back;
    }
    else if (enable_stencil == depth_stencil_mode::outline)
    {
        cci.stencilTestEnable = VK_TRUE;
        cci.back.compareOp = VK_COMPARE_OP_NOT_EQUAL;
        cci.back.failOp = VK_STENCIL_OP_KEEP;
        cci.back.depthFailOp = VK_STENCIL_OP_KEEP;
        cci.back.passOp = VK_STENCIL_OP_REPLACE;
        cci.back.compareMask = 0xff;
        cci.back.writeMask = 0xff;
        cci.back.reference = 1;
        cci.front = cci.back;
        cci.depthTestEnable = VK_FALSE;
    }

    return cci;
}

VkDescriptorSetLayoutBinding
make_descriptor_set_layout_binding(VkDescriptorType type,
                                   VkShaderStageFlags stage_flags,
                                   uint32_t binding)
{
    VkDescriptorSetLayoutBinding b = {};
    b.binding = binding;
    b.descriptorCount = 1;
    b.descriptorType = type;
    b.pImmutableSamplers = nullptr;
    b.stageFlags = stage_flags;

    return b;
}
VkWriteDescriptorSet
make_write_descriptor_buffer(VkDescriptorType type,
                             VkDescriptorSet dst_set,
                             VkDescriptorBufferInfo* buffer_info,
                             uint32_t binding)
{
    VkWriteDescriptorSet set = {};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.pNext = nullptr;
    set.dstBinding = binding;
    set.dstSet = dst_set;
    set.descriptorCount = 1;
    set.descriptorType = type;
    set.pBufferInfo = buffer_info;

    return set;
}

VkWriteDescriptorSet
make_write_descriptor_image(VkDescriptorType type,
                            VkDescriptorSet dst_set,
                            VkDescriptorImageInfo* image_info,
                            uint32_t binding)
{
    VkWriteDescriptorSet set = {};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.pNext = nullptr;
    set.dstBinding = binding;
    set.dstSet = dst_set;
    set.descriptorCount = 1;
    set.descriptorType = type;
    set.pImageInfo = image_info;

    return set;
}

VkSamplerCreateInfo
make_sampler_create_info(
    VkFilter filters, VkSamplerAddressMode sampler_address_mode /*= VK_SAMPLER_ADDRESS_MODE_REPEAT*/)
{
    VkSamplerCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.pNext = nullptr;
    ci.magFilter = filters;
    ci.minFilter = filters;
    ci.addressModeU = sampler_address_mode;
    ci.addressModeV = sampler_address_mode;
    ci.addressModeW = sampler_address_mode;

    return ci;
}

VkPipelineDynamicStateCreateInfo
make_pipeline_dynamic_state_create_info(const std::vector<VkDynamicState>& dynamic_states,
                                        VkPipelineDynamicStateCreateFlags flags /*= 0*/)
{
    VkPipelineDynamicStateCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    ci.pDynamicStates = dynamic_states.data();
    ci.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    ci.flags = flags;

    return ci;
}

void
make_insert_image_memory_barrier(VkCommandBuffer cmdbuffer,
                                 VkImage image,
                                 VkAccessFlags src_access_mask,
                                 VkAccessFlags dst_access_mask,
                                 VkImageLayout old_image_layout,
                                 VkImageLayout new_image_layout,
                                 VkPipelineStageFlags src_stage_mask,
                                 VkPipelineStageFlags dst_stage_mask,
                                 VkImageSubresourceRange subresource_range)
{
    VkImageMemoryBarrier imageMemoryBarrier = make_image_memory_barrier();
    imageMemoryBarrier.srcAccessMask = src_access_mask;
    imageMemoryBarrier.dstAccessMask = dst_access_mask;
    imageMemoryBarrier.oldLayout = old_image_layout;
    imageMemoryBarrier.newLayout = new_image_layout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresource_range;

    vkCmdPipelineBarrier(cmdbuffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1,
                         &imageMemoryBarrier);
}

VkImageMemoryBarrier
make_image_memory_barrier()
{
    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    return imageMemoryBarrier;
}

}  // namespace agea::render::vk_utils
