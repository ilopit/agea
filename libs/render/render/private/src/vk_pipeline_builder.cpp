#include "vulkan_render/vk_pipeline_builder.h"

namespace kryga::render::vk_utils
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
    color_blend_state_ci.attachmentCount = m_color_attachment_count;
    color_blend_state_ci.pAttachments =
        m_color_attachment_count > 0 ? &m_color_blend_attachment : nullptr;

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

    // Apply specialization constants to all shader stages
    VkSpecializationInfo spec_info{};
    std::vector<VkSpecializationMapEntry> spec_map;
    std::vector<uint32_t> spec_data;

    if (!m_spec_constants.empty())
    {
        spec_data.resize(m_spec_constants.size());
        spec_map.resize(m_spec_constants.size());

        for (size_t i = 0; i < m_spec_constants.size(); ++i)
        {
            spec_data[i] = m_spec_constants[i].value;
            spec_map[i].constantID = m_spec_constants[i].constant_id;
            spec_map[i].offset = static_cast<uint32_t>(i * sizeof(uint32_t));
            spec_map[i].size = sizeof(uint32_t);
        }

        spec_info.mapEntryCount = static_cast<uint32_t>(spec_map.size());
        spec_info.pMapEntries = spec_map.data();
        spec_info.dataSize = spec_data.size() * sizeof(uint32_t);
        spec_info.pData = spec_data.data();

        for (auto& stage : m_shader_stages_ci)
        {
            stage.pSpecializationInfo = &spec_info;
        }
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
}  // namespace kryga::render::vk_utils