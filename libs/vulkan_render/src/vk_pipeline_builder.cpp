#include "vulkan_render/vk_pipeline_builder.h"

VkPipeline
pipeline_builder::build_pipeline(VkDevice device, VkRenderPass pass)
{
    // make viewport state from our stored viewport and scissor.
    // at the moment we wont support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo vs = {};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.pNext = nullptr;

    vs.viewportCount = 1;
    vs.pViewports = &m_viewport;
    vs.scissorCount = 1;
    vs.pScissors = &m_scissor;

    // setup dummy color blending. We arent using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo cb = {};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.pNext = nullptr;

    cb.logicOpEnable = VK_FALSE;
    cb.logicOp = VK_LOGIC_OP_COPY;
    cb.attachmentCount = 1;
    cb.pAttachments = &m_color_blend_attachment;

    // build the actual pipeline
    // we now use all of the info structs we have been writing into into this one to create the
    // pipeline
    VkGraphicsPipelineCreateInfo pi = {};
    pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi.pNext = nullptr;

    pi.stageCount = (uint32_t)m_shader_stages.size();
    pi.pStages = m_shader_stages.data();
    pi.pVertexInputState = &m_vertex_input_info;
    pi.pInputAssemblyState = &m_input_assembly;
    pi.pViewportState = &vs;
    pi.pRasterizationState = &m_rasterizer;
    pi.pMultisampleState = &m_multisampling;
    pi.pColorBlendState = &cb;
    pi.pDepthStencilState = &m_depth_stencil;
    pi.layout = m_pipelineLayout;
    pi.renderPass = pass;
    pi.subpass = 0;
    pi.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline new_pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, nullptr, &new_pipeline) !=
        VK_SUCCESS)
    {
        return VK_NULL_HANDLE;  // failed to create graphics pipeline
    }
    else
    {
        return new_pipeline;
    }
}