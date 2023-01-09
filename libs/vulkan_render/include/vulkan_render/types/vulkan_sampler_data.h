#pragma once

#include "vulkan_render/types/vulkan_generic.h"

#include <utils/id.h>

namespace agea
{
namespace render
{
class sampler_data
{
public:
    sampler_data(const ::agea::utils::id& id, vk_device_provider vk_device);
    ~sampler_data();

    const ::agea::utils::id&
    get_id()
    {
        return m_id;
    }

    VkSampler m_sampler = VK_NULL_HANDLE;

private:
    ::agea::utils::id m_id;
    vk_device_provider m_vk_device;
};
}  // namespace render
}  // namespace agea
