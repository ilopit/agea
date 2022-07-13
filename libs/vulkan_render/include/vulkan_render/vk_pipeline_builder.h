#pragma once

#include "vulkan_render_types/vulkan_types.h"


class pipeline_builder
{
public:
    VkPipeline
    build_pipeline(VkDevice device, VkRenderPass pass);

    std::vector<VkPipelineShaderStageCreateInfo> m_shader_stages;
    VkPipelineVertexInputStateCreateInfo m_vertex_input_info;
    VkPipelineInputAssemblyStateCreateInfo m_input_assembly;
    VkViewport m_viewport;
    VkRect2D m_scissor;
    VkPipelineRasterizationStateCreateInfo m_rasterizer;
    VkPipelineColorBlendAttachmentState m_color_blend_attachment;
    VkPipelineMultisampleStateCreateInfo m_multisampling;
    VkPipelineLayout m_pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo m_depth_stencil;
};