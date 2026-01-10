#include "vulkan_render/types/vulkan_sampler_data.h"

#include "vulkan_render/vulkan_render_device.h"

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
        glob::render_device::getr().delete_immediately(
            [=](VkDevice vkd, VmaAllocator va) { vkDestroySampler(vkd, m_sampler, nullptr); });
    }
}

}  // namespace render
}  // namespace kryga
