#pragma once

#include "vulkan_render/utils/vulkan_image.h"

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

class texture_data
{
public:
    texture_data(const ::agea::utils::id& id, vk_device_provider dbp);
    ~texture_data();

    texture_data(const texture_data&) = delete;
    texture_data&
    operator=(const texture_data&) = delete;

    texture_data(texture_data&& other) noexcept;

    texture_data&
    operator=(texture_data&& other) noexcept;

    const ::agea::utils::id&
    get_id() const
    {
        return m_id;
    }

    VkImageView image_view = VK_NULL_HANDLE;
    vk_utils::vulkan_image image;
    texture_format format = texture_format::unknown;

private:
    ::agea::utils::id m_id;
    vk_device_provider m_device = nullptr;
};

}  // namespace render
}  // namespace agea
