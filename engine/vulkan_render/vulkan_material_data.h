#pragma once

#include "vulkan_types.h"
#include "vulkan_shader_data.h"
#include "vulkan_texture_data.h"

namespace agea
{
namespace render
{
struct shader_effect;

struct material_data
{
    std::string id;

    VkDescriptorSet texture_set;
    VkPipeline pipeline;
    shader_effect* effect = nullptr;
    VkSampler sampler;

    ~material_data();
};
}  // namespace render
}  // namespace agea
