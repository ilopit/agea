#pragma once

#include <utils/id.h>

#include "vulkan_render_types/vulkan_types.h"
#include "vulkan_render_types/vulkan_shader_data.h"
#include "vulkan_render_types/vulkan_texture_data.h"
#include "vulkan_render_types/vulkan_gpu_types.h"

namespace agea
{
namespace render
{
struct shader_effect;

struct material_data
{
    float*
    roughness_ptr()
    {
        return &gpu_data.roughness;
    }

    float*
    metallic_ptr()
    {
        return &gpu_data.roughness;
    }

    float*
    gamma_ptr()
    {
        return &gpu_data.roughness;
    }

    float*
    albedo_ptr()
    {
        return &gpu_data.roughness;
    }

    agea::utils::id id;
    VkDescriptorSet texture_set;
    VkPipeline pipeline;
    shader_effect* effect = nullptr;
    VkSampler sampler;
    gpu_material_data gpu_data;
};
}  // namespace render
}  // namespace agea
