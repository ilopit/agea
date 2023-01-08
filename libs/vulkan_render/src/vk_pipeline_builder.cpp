#include "vulkan_render/vk_pipeline_builder.h"

namespace agea::render::vk_utils
{
VkPipeline
pipeline_builder::build(VkDevice device, VkRenderPass pass)
{
    VkPipelineViewportStateCreateInfo viewport_state_ci = {};
    viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_ci.pNext = nullptr;

    viewport_state_ci.viewportCount = 1;
    viewport_state_ci.pViewports = &m_viewport;
    viewport_state_ci.scissorCount = 1;
    viewport_state_ci.pScissors = &m_scissor;

    VkPipelineColorBlendStateCreateInfo color_blend_state_ci = {};
    color_blend_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_ci.pNext = nullptr;
    color_blend_state_ci.logicOpEnable = VK_FALSE;
    color_blend_state_ci.logicOp = VK_LOGIC_OP_COPY;
    color_blend_state_ci.attachmentCount = 1;
    color_blend_state_ci.pAttachments = &m_color_blend_attachment;

    VkGraphicsPipelineCreateInfo pipeline_ci = {};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.pColorBlendState = &color_blend_state_ci;
    pipeline_ci.pNext = nullptr;
    pipeline_ci.pViewportState = &viewport_state_ci;

    if (!m_dynamic_state_enables.empty())
    {
        m_dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        m_dynamic_state_ci.pDynamicStates = m_dynamic_state_enables.data();
        m_dynamic_state_ci.dynamicStateCount =
            static_cast<uint32_t>(m_dynamic_state_enables.size());
        m_dynamic_state_ci.flags = 0;

        pipeline_ci.pDynamicState = &m_dynamic_state_ci;
    }

    pipeline_ci.stageCount = (uint32_t)m_shader_stages_ci.size();
    pipeline_ci.pStages = m_shader_stages_ci.data();
    pipeline_ci.pVertexInputState = &m_vertex_input_info_ci;
    pipeline_ci.pInputAssemblyState = &m_input_assembly_ci;
    pipeline_ci.pRasterizationState = &m_rasterizer_ci;
    pipeline_ci.pMultisampleState = &m_multisampling_ci;
    pipeline_ci.pDepthStencilState = &m_depth_stencil_ci;
    pipeline_ci.layout = m_pipeline_layout;
    pipeline_ci.renderPass = pass;
    pipeline_ci.subpass = 0;
    pipeline_ci.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &pipeline) !=
        VK_SUCCESS)
    {
        return VK_NULL_HANDLE;  // failed to create graphics pipeline
    }
    else
    {
        return pipeline;
    }
}
}  // namespace agea::render::vk_utils