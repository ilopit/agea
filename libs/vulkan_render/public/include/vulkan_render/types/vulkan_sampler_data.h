#pragma once

#include "vulkan_render/types/vulkan_generic.h"

#include <utils/id.h>

namespace kryga
{
namespace render
{
class sampler_data
{
public:
    sampler_data(const ::kryga::utils::id& id);
    ~sampler_data();

    const ::kryga::utils::id&
    get_id()
    {
        return m_id;
    }

    VkSampler m_sampler = VK_NULL_HANDLE;

private:
    ::kryga::utils::id m_id;
};
}  // namespace render
}  // namespace kryga
