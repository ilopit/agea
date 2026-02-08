#include "vulkan_render/types/vulkan_compute_shader_data.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/types/vulkan_shader_data.h"

#include <global_state/global_state.h>

namespace kryga
{
namespace render
{

compute_shader_data::compute_shader_data(const ::kryga::utils::id& id)
    : m_id(id)
{
    m_set_layout.fill(VK_NULL_HANDLE);
}

compute_shader_data::~compute_shader_data()
{
    reset();
}

void
compute_shader_data::reset()
{
    auto device = glob::glob_state().get_render_device();
    if (!device)
        return;

    if (m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device->vk_device(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_pipeline_layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device->vk_device(), m_pipeline_layout, nullptr);
        m_pipeline_layout = VK_NULL_HANDLE;
    }

    for (auto& layout : m_set_layout)
    {
        if (layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(device->vk_device(), layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }

    m_compute_stage.reset();
}

}  // namespace render
}  // namespace kryga
