#pragma once

#include "vulkan_render/utils/vulkan_image.h"

#include <utils/id.h>

namespace kryga
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
    texture_data(const ::kryga::utils::id& id);
    ~texture_data();

    texture_data(const texture_data&) = delete;
    texture_data&
    operator=(const texture_data&) = delete;

    texture_data(texture_data&& other) noexcept;

    texture_data&
    operator=(texture_data&& other) noexcept;

    const ::kryga::utils::id&
    get_id() const
    {
        return m_id;
    }

    vk_utils::vulkan_image_sptr image;
    vk_utils::vulkan_image_view_sptr image_view;
    texture_format format = texture_format::unknown;

private:
    ::kryga::utils::id m_id;
};

}  // namespace render
}  // namespace kryga
