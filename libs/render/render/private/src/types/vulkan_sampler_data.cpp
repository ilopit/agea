#include "vulkan_render/types/vulkan_sampler_data.h"

#include "vulkan_render/vulkan_render_device.h"

#include <global_state/global_state.h>

namespace kryga
{
namespace render
{
sampler_data::sampler_data(const ::kryga::utils::id& id)
    : m_id(id)
{
}

sampler_data::~sampler_data()
{
    if (m_sampler)
    {
        glob::glob_state().getr_render_device().schedule_to_delete(
            [s = m_sampler](VkDevice vkd, VmaAllocator) { vkDestroySampler(vkd, s, nullptr); });
    }
}

}  // namespace render
}  // namespace kryga
