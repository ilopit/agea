#pragma once

#include "vulkan_render/types/vulkan_types.h"

namespace agea::render::vk_utils
{

class pipeline_builder
{
public:
    VkPipeline
    build(VkDevice device, VkRenderPass pass);

    VkViewport m_viewport{};
    VkRect2D m_scissor{};

    VkPipelineRasterizationStateCreateInfo m_rasterizer_ci{};
    std::vector<VkPipelineShaderStageCreateInfo> m_shader_stages_ci{};
    VkPipelineVertexInputStateCreateInfo m_vertex_input_info_ci{};
    VkPipelineInputAssemblyStateCreateInfo m_input_assembly_ci{};
    VkPipelineDynamicStateCreateInfo m_dynamic_state_ci{};
    VkPipelineColorBlendAttachmentState m_color_blend_attachment{};
    VkPipelineMultisampleStateCreateInfo m_multisampling_ci{};
    VkPipelineDepthStencilStateCreateInfo m_depth_stencil_ci{};
    std::vector<VkDynamicState> m_dynamic_state_enables{};

    VkPipelineLayout m_pipeline_layout{};
};
}  // namespace agea::render::vk_utils
