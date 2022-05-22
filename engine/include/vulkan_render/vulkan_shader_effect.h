// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vulkan_types.h"
#include "core/vk_descriptors.h"

#include <vector>
#include <array>
#include <unordered_map>
#include <memory>

namespace agea
{
namespace render
{
class render_device;
struct shader_data;

uint32_t
hash_descriptor_layout_info(VkDescriptorSetLayoutCreateInfo* info);

class vulkan_engine;
// holds all information for a given shader set for pipeline
struct shader_effect
{
    struct reflection_overrides
    {
        const char* m_name;
        VkDescriptorType m_overridenType;
    };

    void
    add_stage(shader_data* shaderModule, VkShaderStageFlagBits stage);

    void
    reflect_layout(render_device* engine, reflection_overrides* overrides, int overrideCount);
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

private:
    struct shader_stage
    {
        shader_data* m_shaderModule = nullptr;
        VkShaderStageFlagBits m_stage;
    };

    std::vector<shader_stage> m_stages;
};

}  // namespace render
}  // namespace agea
