#pragma once

#include "vulkan_render/types/vulkan_generic.h"

#include <utils/id.h>
#include <utils/dynamic_object.h>
#include <utils/dynamic_object_builder.h>

#include <vector>
#include <array>
#include <unordered_map>

namespace agea
{
namespace render
{
class shader_data;

struct vulkan_descriptor_set_layout_data
{
    uint32_t set_number = 0;
    VkDescriptorSetLayoutCreateInfo create_info;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

namespace reflection
{
struct descriptor_binding
{
    VkDescriptorSetLayoutBinding
    as_vk()
    {
        VkDescriptorSetLayoutBinding layout_binding;

        layout_binding.binding = binding;
        layout_binding.stageFlags = stage;
        layout_binding.descriptorCount = descriptors_count;
        layout_binding.descriptorType = descriptor_type;
        layout_binding.pImmutableSamplers = nullptr;

        return layout_binding;
    }

    std::string name;
    uint32_t set;
    uint32_t binding;
    uint32_t descriptors_count;
    bool is_array = false;
    VkDescriptorType descriptor_type;
    VkShaderStageFlagBits stage;
    ::agea::utils::dynobj_layout_sptr layout;
};

struct descriptor_set
{
    void
    get_descriptor_set_layout_data(vulkan_descriptor_set_layout_data& layout)
    {
        layout.bindings.resize(binding.size());
        for (size_t i = 0; i < binding.size(); ++i)
        {
            layout.bindings[i] = binding[i].as_vk();
        }

        layout.set_number = set;
        layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.create_info.bindingCount = (uint32_t)binding.size();
        layout.create_info.pBindings = layout.bindings.data();
    }

    uint32_t set;
    std::vector<descriptor_binding> binding;
};

struct push_constants
{
    VkPushConstantRange
    as_vk()
    {
        VkPushConstantRange pcs{};
        pcs.offset = offset;
        pcs.size = size;
        pcs.stageFlags = stage;

        return pcs;
    }

    std::string name;

    uint32_t offset;
    uint32_t size;
    VkShaderStageFlagBits stage;
};

struct shader_reflection
{
    push_constants constants;
    std::vector<reflection::descriptor_set> sets;

    agea::utils::dynobj_layout_sptr constants_layout;
    agea::utils::dynobj_layout_sptr input_layout;
    agea::utils::dynobj_layout_sptr output_layout;
};

}  // namespace reflection

class shader_effect_data
{
public:
    shader_effect_data(const ::agea::utils::id& id, vk_device_provider vdp);
    shader_effect_data(const ::agea::utils::id& id,
                       vk_device_provider vdp,
                       const utils::dynobj_layout_sptr& v);

    ~shader_effect_data();

    const ::agea::utils::id&
    get_id() const
    {
        return m_id;
    }

    void
    reset();

    void
    generate_set_layouts(std::vector<vulkan_descriptor_set_layout_data>& set_layouts);

    void
    generate_constants(std::vector<VkPushConstantRange>& constants);

    void
    set_expected_vertex_input(const utils::dynobj_layout_sptr& v)
    {
        m_expected_vertex_input = v;
    }

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

    std::array<VkDescriptorSetLayout, DESCRIPTORS_SETS_COUNT> m_set_layout;

    utils::dynobj_layout_sptr m_expected_vertex_input;

    std::shared_ptr<shader_data> m_vertex_stage;
    reflection::shader_reflection m_vertext_stage_reflection;

    std::shared_ptr<shader_data> m_frag_stage;
    reflection::shader_reflection m_frag_stage_reflection;

    bool m_is_wire = false;
    bool m_enable_alpha = false;
    bool m_system = false;
    bool m_failed_load = false;

private:
    vk_device_provider m_device;
    ::agea::utils::id m_id;
};

}  // namespace render
}  // namespace agea
