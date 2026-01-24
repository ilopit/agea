#pragma once

#include "vulkan_render/types/vulkan_generic.h"

#include <shader_system/shader_reflection.h>

#include <utils/id.h>

#include <memory>
#include <array>

namespace kryga
{
namespace render
{
class shader_module_data;
class render_pass;

// Pure compute shader container - analogous to shader_effect_data for graphics.
// Bindings are owned by the parent render_pass, not by this class.
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

    render_pass*
    get_owner_pass() const
    {
        return m_owner_pass;
    }

    void
    set_owner_pass(render_pass* pass)
    {
        m_owner_pass = pass;
    }

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

    std::array<VkDescriptorSetLayout, DESCRIPTORS_SETS_COUNT> m_set_layout;

    std::shared_ptr<shader_module_data> m_compute_stage;

    bool m_failed_load = false;

private:
    ::kryga::utils::id m_id;
    render_pass* m_owner_pass = nullptr;
};

}  // namespace render
}  // namespace kryga
