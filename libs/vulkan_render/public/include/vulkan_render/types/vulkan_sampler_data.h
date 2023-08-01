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
    sampler_data(const ::agea::utils::id& id);
    ~sampler_data();

    const ::agea::utils::id&
    get_id()
    {
        return m_id;
    }

    VkSampler m_sampler = VK_NULL_HANDLE;

private:
    ::agea::utils::id m_id;
};
}  // namespace render
}  // namespace agea
