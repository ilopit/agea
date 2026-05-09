#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include "vulkan_render/vulkan_render_device.h"

#include <global_state/global_state.h>

#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/shader_reflection_utils.h"

#include <utils/kryga_log.h>

namespace kryga
{
namespace render
{

shader_effect_data::shader_effect_data(const ::kryga::utils::id& id)
    : m_id(id)
    , m_expected_vertex_input(get_default_vertex_inout_layout())
{
    m_set_layout.fill(VK_NULL_HANDLE);
}

shader_effect_data::shader_effect_data(const ::kryga::utils::id& id,
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
        glob::glob_state().getr_render_device().schedule_to_delete(
            [owns_layout = m_owns_pipeline_layout,
             set_layout = m_set_layout,
             pl = m_pipeline_layout,
             pipe = m_pipeline,
             stencil_pipe = m_with_stencil_pipeline](VkDevice vd, VmaAllocator)
            {
                if (owns_layout)
                {
                    for (auto l : set_layout)
                    {
                        vkDestroyDescriptorSetLayout(vd, l, nullptr);
                    }
                    vkDestroyPipelineLayout(vd, pl, nullptr);
                }

                vkDestroyPipeline(vd, pipe, nullptr);
                vkDestroyPipeline(vd, stencil_pipe, nullptr);
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

    for (auto& s : m_vertex_stage->get_reflection().descriptors)
    {
        vulkan_shader_reflection_utils::convert_to_ds_layout_data(
            s, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, set_layouts.emplace_back());
    }

    if (m_frag_stage)
    {
        for (auto& s : m_frag_stage->get_reflection().descriptors)
        {
            vulkan_shader_reflection_utils::convert_to_ds_layout_data(
                s, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT, set_layouts.emplace_back());
        }
    }
}

void
shader_effect_data::generate_constants(std::vector<VkPushConstantRange>& constants)
{
    // Project convention: both shader stages declare the same push-constant
    // struct using `layout(push_constant, scalar)`. C++ structs match scalar
    // layout because `bda_addr` has no `alignas`. The two reflections must
    // therefore agree on (offset, size); if they don't, a stage is using a
    // different layout decoration (e.g. std430 mixed with scalar) — assert
    // here so the mismatch fails at shader effect creation rather than as
    // a silent UB at draw time.
    const reflection::push_constants* vert =
        (m_vertex_stage && m_vertex_stage->get_reflection().constants)
            ? &*m_vertex_stage->get_reflection().constants
            : nullptr;
    const reflection::push_constants* frag =
        (m_frag_stage && m_frag_stage->get_reflection().constants)
            ? &*m_frag_stage->get_reflection().constants
            : nullptr;

    if (!vert && !frag)
    {
        return;
    }

    VkShaderStageFlags stages = 0;
    uint32_t offset = 0;
    uint32_t size = 0;

    if (vert)
    {
        stages |= VK_SHADER_STAGE_VERTEX_BIT;
        offset = vert->offset;
        size = vert->size;
    }
    if (frag)
    {
        stages |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (vert)
        {
            if (vert->offset != frag->offset || vert->size != frag->size)
            {
                ALOG_ERROR("Push constant layout mismatch on shader effect '{}': vert "
                           "(offset={}, size={}) != frag (offset={}, size={}). Both stages "
                           "must use `layout(push_constant, scalar)` and declare the same struct.",
                           m_id.cstr(),
                           vert->offset,
                           vert->size,
                           frag->offset,
                           frag->size);
                KRG_check(false, "Push constant layout mismatch between stages");
            }
        }
        else
        {
            offset = frag->offset;
            size = frag->size;
        }
    }

    VkPushConstantRange r{};
    r.stageFlags = stages;
    r.offset = offset;
    r.size = size;
    constants.push_back(r);
}

void
shader_effect_data::push_constants(VkCommandBuffer cmd, const void* data) const
{
    for (const auto& range : m_push_constant_ranges)
    {
        vkCmdPushConstants(cmd,
                           m_pipeline_layout,
                           range.stageFlags,
                           range.offset,
                           range.size,
                           static_cast<const uint8_t*>(data) + range.offset);
    }
}

}  // namespace render
}  // namespace kryga
