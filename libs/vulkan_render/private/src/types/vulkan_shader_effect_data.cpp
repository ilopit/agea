#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include "vulkan_render/vulkan_render_device.h"

#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/shader_reflection_utils.h"

#include <utils/agea_log.h>

namespace agea
{
namespace render
{

shader_effect_data::shader_effect_data(const ::agea::utils::id& id)
    : m_id(id)
    , m_expected_vertex_input(get_default_vertex_inout_layout())
{
    m_set_layout.fill(VK_NULL_HANDLE);
}

shader_effect_data::shader_effect_data(const ::agea::utils::id& id,
                                       const utils::dynobj_layout_sptr& v)
    : m_id(id)
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

    if (m_pipeline)
    {
        glob::render_device::getr().delete_immediately(
            [=](VkDevice vd, VmaAllocator)
            {
                for (auto l : m_set_layout)
                {
                    vkDestroyDescriptorSetLayout(vd, l, nullptr);
                }

                vkDestroyPipeline(vd, m_pipeline, nullptr);
                vkDestroyPipeline(vd, m_with_stencil_pipeline, nullptr);
                vkDestroyPipelineLayout(vd, m_pipeline_layout, nullptr);
            });

        for (size_t i = 0; i < DESCRIPTORS_SETS_COUNT; ++i)
        {
            m_set_layout[i] = VK_NULL_HANDLE;
        }

        m_pipeline = VK_NULL_HANDLE;
        m_with_stencil_pipeline = VK_NULL_HANDLE;
        m_pipeline_layout = VK_NULL_HANDLE;
    }
}

void
shader_effect_data::generate_set_layouts(
    std::vector<vulkan_descriptor_set_layout_data>& set_layouts)
{
    set_layouts.clear();

    for (auto& s : m_vertext_stage_reflection.descriptors)
    {
        shader_reflection_utils::convert_to_ds_layout_data(
            s, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, set_layouts.emplace_back());
    }

    for (auto& s : m_fragment_stage_reflection.descriptors)
    {
        shader_reflection_utils::convert_to_ds_layout_data(
            s, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT, set_layouts.emplace_back());
    }
}

void
shader_effect_data::generate_constants(std::vector<VkPushConstantRange>& constants)
{
    for (auto& c : m_vertext_stage_reflection.constants)
    {
        shader_reflection_utils::convert_to_vk_push_constants(
            c, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, constants.emplace_back());
    }

    for (auto& c : m_fragment_stage_reflection.constants)
    {
        shader_reflection_utils::convert_to_vk_push_constants(
            c, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT, constants.emplace_back());
    }
}

}  // namespace render
}  // namespace agea
