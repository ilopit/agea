// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vulkan_render_types/vulkan_types.h"

#include <vector>
#include <array>
#include <unordered_map>
#include <memory>

namespace agea
{
namespace render
{
struct shader_data;

struct shader_effect
{
    struct reflection_overrides
    {
        const char* m_name;
        VkDescriptorType m_overridenType;
    };

    void
    add_stage(shader_data* shaderModule, VkShaderStageFlagBits stage);

    VkPipelineLayout m_build_layout;

    struct reflected_binding
    {
        int m_set;
        int m_binding;
        VkDescriptorType m_type;
    };

    std::unordered_map<std::string, reflected_binding> m_bindings;
    std::array<VkDescriptorSetLayout, 4> m_set_layouts;
    std::array<uint32_t, 4> m_set_hashes;

    struct shader_stage
    {
        shader_data* m_shaderModule = nullptr;
        VkShaderStageFlagBits m_stage;
    };

    std::vector<shader_stage> m_stages;
};

}  // namespace render
}  // namespace agea
