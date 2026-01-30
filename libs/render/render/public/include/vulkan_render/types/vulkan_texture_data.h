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
    static constexpr uint32_t INVALID_BINDLESS_INDEX = UINT32_MAX;

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

    uint32_t
    get_bindless_index() const
    {
        return m_bindless_index;
    }

    void
    set_bindless_index(uint32_t index)
    {
        m_bindless_index = index;
    }

    vk_utils::vulkan_image_sptr image;
    vk_utils::vulkan_image_view_sptr image_view;
    texture_format format = texture_format::unknown;

private:
    ::kryga::utils::id m_id;
    uint32_t m_bindless_index = INVALID_BINDLESS_INDEX;
};

}  // namespace render
}  // namespace kryga
