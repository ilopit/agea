#pragma once

#include "vulkan_render/types/vulkan_render_types_fwds.h"

#include <utils/buffer.h>

#include <vulkan/vulkan.h>

namespace agea
{
namespace render
{
struct shader_effect_create_info
{
    agea::utils::buffer* vert_buffer = nullptr;
    bool is_vert_binary = false;
    agea::utils::buffer* frag_buffer = nullptr;
    bool is_frag_binary = false;
    bool is_wire = false;
    bool enable_alpha = false;
    bool enable_dynamic_state = false;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    const vertex_input_description* vert_input_description = nullptr;
    VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
};
}  // namespace render
}  // namespace agea