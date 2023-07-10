#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/shader_reflection_utils.h"

#include "utils/agea_log.h"

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
    set_layouts.clear();

    {
        auto v = m_vertext_stage_reflection.descriptor_sets->make_view<gpu_type>();

        auto set_count = v.field_count();

        for (uint64_t set_idx = 0; set_idx < set_count; ++set_idx)
        {
            auto& layout = set_layouts.emplace_back();

            shader_reflection_utils::convert_dynobj_to_layout_data(v.subobj(set_idx), layout);
        }
    }

    {
        auto v = m_frag_stage_reflection.descriptor_sets->make_view<gpu_type>();

        auto set_count = v.field_count();

        for (uint64_t set_idx = 0; set_idx < set_count; ++set_idx)
        {
            auto& layout = set_layouts.emplace_back();

            shader_reflection_utils::convert_dynobj_to_layout_data(v.subobj(set_idx), layout);
        }
    }
}

void
shader_effect_data::generate_constants(std::vector<VkPushConstantRange>& constants)
{
    if (m_vertext_stage_reflection.constants_layout)
    {
        auto v = m_vertext_stage_reflection.constants_layout->make_view<gpu_type>();
        {
            auto& push_range = constants.emplace_back();

            shader_reflection_utils::convert_dynobj_to_vk_push_constants(v.subobj(0), push_range);
        }
    }

    if (m_frag_stage_reflection.constants_layout)
    {
        auto v = m_frag_stage_reflection.constants_layout->make_view<gpu_type>();
        {
            auto& push_range = constants.emplace_back();

            shader_reflection_utils::convert_dynobj_to_vk_push_constants(v.subobj(0), push_range);
        }
    }
}

}  // namespace render
}  // namespace agea
