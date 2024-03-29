#pragma once

#include "vulkan_render/types/vulkan_shader_effects_presets.h"
#include <utils/buffer.h>

#include <vulkan/vulkan.h>

namespace agea
{
namespace render
{

class render_pass;

struct shader_effect_create_info
{
    agea::utils::buffer* vert_buffer = nullptr;
    bool is_vert_binary = false;
    agea::utils::buffer* frag_buffer = nullptr;
    bool is_frag_binary = false;
    bool is_wire = false;
    alpha_mode alpha = alpha_mode::none;
    bool enable_dynamic_state = false;
    utils::dynobj_layout_sptr expected_input_vertex_layout;
    render_pass* rp = VK_NULL_HANDLE;
    VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
    depth_stencil_mode ds_mode = depth_stencil_mode::none;

    uint32_t width = 0;
    uint32_t height = 0;
};
}  // namespace render
}  // namespace agea