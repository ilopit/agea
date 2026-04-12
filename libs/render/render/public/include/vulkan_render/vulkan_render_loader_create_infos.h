#pragma once

#include "vulkan_render/types/vulkan_shader_effects_presets.h"
#include <utils/buffer.h>

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace kryga
{
namespace render
{

class render_pass;

struct shader_effect_create_info
{
    kryga::utils::buffer* vert_buffer = nullptr;
    bool is_vert_binary = false;
    kryga::utils::buffer* frag_buffer = nullptr;
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

    // Preprocessor defines passed to glslc (e.g., "ENABLE_LIGHTMAP=1")
    std::vector<std::string> defines;

    // If set, reuse this pipeline layout instead of building from reflection.
    // The shader effect will NOT own/destroy the layout.
    VkPipelineLayout shared_pipeline_layout = VK_NULL_HANDLE;
};

struct compute_shader_create_info
{
    kryga::utils::buffer* shader_buffer = nullptr;
    bool is_binary = false;
    render_pass* pass = nullptr;
};

}  // namespace render
}  // namespace kryga