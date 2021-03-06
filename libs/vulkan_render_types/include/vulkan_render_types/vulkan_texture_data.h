#pragma once

#include "vulkan_render_types/vulkan_types.h"

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
    texture_data() = default;
    ~texture_data();

    texture_data(const texture_data&) = delete;
    texture_data(texture_data&&) = delete;

    texture_data&
    operator=(const texture_data&) = delete;
    texture_data&
    operator=(texture_data&&) = delete;

    vk_device_provider m_device;
    allocated_image image;
    VkImageView image_view;
    texture_format format = texture_format::unknown;
};

}  // namespace render
}  // namespace agea
