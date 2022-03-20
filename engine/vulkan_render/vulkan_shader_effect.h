// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vulkan_types.h"
#include "vk_descriptors.h"

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

uint32_t hash_descriptor_layout_info(VkDescriptorSetLayoutCreateInfo* info);

class vulkan_engine;
// holds all information for a given shader set for pipeline
struct shader_effect
{
    struct reflection_overrides
    {
        const char* m_name;
        VkDescriptorType m_overridenType;
    };

    void add_stage(shader_data* shaderModule, VkShaderStageFlagBits stage);

    void reflect_layout(render_device* engine, reflection_overrides* overrides, int overrideCount);
    VkPipelineLayout m_build_layout;

    struct reflected_binding
    {
        uint32_t m_set;
        uint32_t m_binding;
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

struct shader_descriptor_binder
{
    struct buffer_write_descriptor
    {
        int m_dstSet;
        int m_dstBinding;
        VkDescriptorType m_descriptor_type;
        VkDescriptorBufferInfo m_bufferInfo;

        uint32_t m_dynamic_offset;
    };

    void bind_buffer(const char* name, const VkDescriptorBufferInfo& bufferInfo);

    void bind_dynamic_buffer(const char* name,
                             uint32_t offset,
                             const VkDescriptorBufferInfo& bufferInfo);

    void apply_binds(VkCommandBuffer cmd);

    void build_sets(VkDevice device, vk_utils::descriptor_allocator& allocator);

    void set_shader(shader_effect* newShader);

    std::array<VkDescriptorSet, 4> m_cached_descriptor_sets;

private:
    struct dyn_offsets
    {
        std::array<uint32_t, 16> m_offsets;
        uint32_t count{0};
    };
    std::array<dyn_offsets, 4> m_set_offsets;

    shader_effect* m_shaders{nullptr};
    std::vector<buffer_write_descriptor> m_buffer_writes;
};
}  // namespace render
}  // namespace agea
