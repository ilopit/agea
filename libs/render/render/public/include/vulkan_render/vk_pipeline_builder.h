#pragma once

#include "vulkan_render/types/vulkan_generic.h"

#include <cstdint>
#include <unordered_map>

namespace kryga::render::vk_utils
{

// Specialization constant entry (id → value)
struct spec_constant_entry
{
    uint32_t constant_id;
    uint32_t value;  // VkBool32 for bools, uint32_t for ints
};

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
    uint32_t m_color_attachment_count = 1;

    // Specialization constants applied to all stages
    std::vector<spec_constant_entry> m_spec_constants;
};
}  // namespace kryga::render::vk_utils
