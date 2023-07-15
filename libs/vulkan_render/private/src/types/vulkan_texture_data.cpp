#include "vulkan_render/types/vulkan_texture_data.h"

namespace agea
{
namespace render
{

texture_data::texture_data(const ::agea::utils::id& id, vk_device_provider vdp)
    : m_id(id)
    , m_device(vdp)
{
}

texture_data::texture_data(texture_data&& other) noexcept
    : image_view(other.image_view)
    , image(std::move(other.image))
    , format(other.format)
    , m_id(std::move(other.m_id))
    , m_device(std::move(m_device))
{
    other.image_view = VK_NULL_HANDLE;
    other.format = texture_format::unknown;
}

texture_data&
texture_data::operator=(texture_data&& other) noexcept
{
    if (this != &other)
    {
        image_view = other.image_view;
        other.image_view = VK_NULL_HANDLE;

        image = std::move(other.image);

        format = other.format;
        other.format = texture_format::unknown;

        m_id = std::move(other.m_id);
        m_device = std::move(m_device);
    }

    return *this;
}

texture_data::~texture_data()
{
    if (m_device)
    {
        vkDestroyImageView(m_device(), image_view, nullptr);
    }
}
}  // namespace render
}  // namespace agea
