#pragma once

#include <utils/id.h>

#include "vulkan_types.h"
#include "vulkan_shader_data.h"
#include "vulkan_texture_data.h"

namespace agea
{
namespace render
{
struct shader_effect;

struct gpu_material_data
{
    float roughness = 0.5f;
    float metallic = 0.5f;
    float gamma = 0.8f;
    float albedo = 0.2f;
};

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

    utils::id id;

    VkDescriptorSet texture_set;
    VkPipeline pipeline;
    shader_effect* effect = nullptr;
    VkSampler sampler;

    gpu_material_data gpu_data;
};
}  // namespace render
}  // namespace agea
