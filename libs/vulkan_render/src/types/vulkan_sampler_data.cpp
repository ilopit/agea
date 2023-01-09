#include "vulkan_render/types/vulkan_sampler_data.h"

namespace agea
{
namespace render
{
sampler_data::sampler_data(const ::agea::utils::id& id, vk_device_provider vk_device)
    : m_id(id)
    , m_vk_device(vk_device)
{
}

sampler_data::~sampler_data()
{
    vkDestroySampler(m_vk_device(), m_sampler, nullptr);
}

}  // namespace render
}  // namespace agea
