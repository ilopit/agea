#pragma once

#include "vulkan_render/types/vulkan_generic.h"
#include "vulkan_render/types/binding_table.h"
#include "vulkan_render/shader_reflection_utils.h"

#include <utils/id.h>

#include <memory>
#include <array>

namespace kryga
{
namespace render
{
class shader_module_data;
class vulkan_render_graph;

class compute_shader_data
{
public:
    explicit compute_shader_data(const ::kryga::utils::id& id);
    ~compute_shader_data();

    const ::kryga::utils::id&
    get_id() const
    {
        return m_id;
    }

    void
    reset();

    // === Binding table API ===

    binding_table&
    bindings()
    {
        KRG_check(!m_binding_table.is_finalized(), "Cannot modify finalized bindings");
        return m_binding_table;
    }

    const binding_table&
    bindings() const
    {
        return m_binding_table;
    }

    bool
    finalize_bindings(vk_utils::descriptor_layout_cache& layout_cache)
    {
        return m_binding_table.finalize(layout_cache);
    }

    bool
    validate_resources(const vulkan_render_graph& graph) const
    {
        return m_binding_table.validate_resources(graph);
    }

    bool
    are_bindings_finalized() const
    {
        return m_binding_table.is_finalized();
    }

    void
    bind(const utils::id& name, vk_utils::vulkan_buffer& buf)
    {
        m_binding_table.bind_buffer(name, buf);
    }

    VkDescriptorSet
    get_descriptor_set(uint32_t set_index, vk_utils::descriptor_allocator& allocator)
    {
        return m_binding_table.build_set(set_index, allocator);
    }

    void
    begin_frame()
    {
        m_binding_table.begin_frame();
    }

    VkDescriptorSetLayout
    get_set_layout(uint32_t set_index) const
    {
        return m_binding_table.get_layout(set_index);
    }

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

    std::array<VkDescriptorSetLayout, DESCRIPTORS_SETS_COUNT> m_set_layout;

    std::shared_ptr<shader_module_data> m_compute_stage;
    reflection::shader_reflection m_compute_stage_reflection;

private:
    ::kryga::utils::id m_id;
    binding_table m_binding_table;
};

}  // namespace render
}  // namespace kryga
