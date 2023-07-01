#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"

namespace agea
{
namespace render
{

shader_effect_data::shader_effect_data(const ::agea::utils::id& id, vk_device_provider vdp)
    : m_id(id)
    , m_device(vdp)
    , m_expected_vertex_input(get_default_vertex_inout_layout())
{
    m_set_layout.fill(VK_NULL_HANDLE);
}

shader_effect_data::shader_effect_data(const ::agea::utils::id& id,
                                       vk_device_provider vdp,
                                       const utils::dynobj_layout_sptr& v)
    : m_id(id)
    , m_device(vdp)
    , m_expected_vertex_input(v)
{
    m_set_layout.fill(VK_NULL_HANDLE);
}

shader_effect_data::~shader_effect_data()
{
    reset();
}

void
shader_effect_data::reset()
{
    m_vertex_stage.reset();
    m_frag_stage.reset();

    for (auto l : m_set_layout)
    {
        vkDestroyDescriptorSetLayout(m_device(), l, nullptr);
    }

    vkDestroyPipeline(m_device(), m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device(), m_pipeline_layout, nullptr);

    for (size_t i = 0; i < DESCRIPTORS_SETS_COUNT; ++i)
    {
        m_set_layout[i] = VK_NULL_HANDLE;
    }

    m_pipeline = VK_NULL_HANDLE;
    m_pipeline_layout = VK_NULL_HANDLE;
}

void
shader_effect_data::generate_set_layouts(
    std::vector<vulkan_descriptor_set_layout_data>& set_layouts)
{
    size_t size = 0;

    size += m_vertext_stage_reflection.sets.size();
    size += m_frag_stage_reflection.sets.size();

    set_layouts.clear();

    for (auto& s : m_vertext_stage_reflection.sets)
    {
        s.get_descriptor_set_layout_data(set_layouts.emplace_back());
    }

    for (auto& s : m_frag_stage_reflection.sets)
    {
        s.get_descriptor_set_layout_data(set_layouts.emplace_back());
    }
}

void
shader_effect_data::generate_constants(std::vector<VkPushConstantRange>& constants)
{
    if (!m_vertext_stage_reflection.constants.name.empty())
    {
        constants.push_back(m_vertext_stage_reflection.constants.as_vk());
    }

    if (!m_frag_stage_reflection.constants.name.empty())
    {
        constants.push_back(m_frag_stage_reflection.constants.as_vk());
    }
}

}  // namespace render
}  // namespace agea
