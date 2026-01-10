#pragma once

#include "vulkan_render/types/vulkan_generic.h"
#include "vulkan_render/shader_reflection_utils.h"

#include <utils/id.h>

#include <memory>
#include <array>

namespace kryga
{
namespace render
{
class shader_module_data;

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

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

    std::array<VkDescriptorSetLayout, DESCRIPTORS_SETS_COUNT> m_set_layout;

    std::shared_ptr<shader_module_data> m_compute_stage;
    reflection::shader_reflection m_compute_stage_reflection;

private:
    ::kryga::utils::id m_id;
};

}  // namespace render
}  // namespace kryga
