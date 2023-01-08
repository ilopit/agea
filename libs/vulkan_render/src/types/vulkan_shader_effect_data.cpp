#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include "vulkan_render/types/vulkan_shader_data.h"

namespace agea
{
namespace render
{

shader_effect_data::shader_effect_data(const ::agea::utils::id& id, vk_device_provider vdp)
    : m_id(id)
    , m_device(vdp)
{
}

shader_effect_data::~shader_effect_data()
{
    reset();
}

void
shader_effect_data::reset()
{
    m_stages.clear();

    for (auto l : m_set_layout)
    {
        vkDestroyDescriptorSetLayout(m_device(), l, nullptr);
    }

    vkDestroyPipeline(m_device(), m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device(), m_pipeline_layout, nullptr);

    m_reflection.clear();

    for (size_t i = 0; i < DESCRIPTORS_SETS_COUNT; ++i)
    {
        m_set_layout[i] = VK_NULL_HANDLE;
    }

    m_pipeline = VK_NULL_HANDLE;
    m_pipeline_layout = VK_NULL_HANDLE;
}

void
shader_effect_data::add_shader(std::shared_ptr<shader_data> se_data)
{
    m_stages[se_data->stage()] = se_data;
}

void
shader_effect_data::generate_set_layouts(
    std::vector<vulkan_descriptor_set_layout_data>& set_layouts)
{
    size_t size = 0;

    for (auto& shader_reflection : m_reflection)
    {
        size += shader_reflection.second.sets.size();
    }
    set_layouts.clear();

    for (auto& shader_reflection : m_reflection)
    {
        for (auto& s : shader_reflection.second.sets)
        {
            s.get_descriptor_set_layout_data(set_layouts.emplace_back());
        }
    }
}

void
shader_effect_data::generate_constants(std::vector<VkPushConstantRange>& constants)
{
    for (auto& sr : m_reflection)
    {
        if (!sr.second.constants.name.empty())
        {
            constants.push_back(sr.second.constants.as_vk());
        }
    }
}

}  // namespace render
}  // namespace agea
