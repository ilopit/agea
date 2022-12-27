#pragma once

#include "vulkan_render_types/vulkan_types.h"

#include <utils/id.h>

namespace agea
{
namespace render
{
enum class texture_format : uint32_t
{
    unknown = 0,
    rgba8
};

struct texture_data
{
    texture_data(const ::agea::utils::id& id, vk_device_provider dbp);
    ~texture_data();

    texture_data(const texture_data&) = delete;
    texture_data(texture_data&&) = delete;
    texture_data&
    operator=(const texture_data&) = delete;
    texture_data&
    operator=(texture_data&&) = delete;

    VkImageView image_view = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    allocated_image image;

    vk_device_provider m_device = nullptr;
    texture_format format = texture_format::unknown;

    const ::agea::utils::id&
    id() const
    {
        return m_id;
    }

    ::agea::utils::id m_id;
};

}  // namespace render
}  // namespace agea
