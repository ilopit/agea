#include "vulkan_render/types/vulkan_texture_data.h"

namespace agea
{
namespace render
{

texture_data::texture_data(const ::agea::utils::id& id)
    : m_id(id)
{
}

texture_data::texture_data(texture_data&& other) noexcept
    : image_view(other.image_view)
    , image(std::move(other.image))
    , format(other.format)
    , m_id(std::move(other.m_id))
{
    other.format = texture_format::unknown;
}

texture_data&
texture_data::operator=(texture_data&& other) noexcept
{
    if (this != &other)
    {
        image_view = std::move(other.image_view);
        image = std::move(other.image);

        format = other.format;
        other.format = texture_format::unknown;

        m_id = std::move(other.m_id);
    }

    return *this;
}

texture_data::~texture_data()
{
}
}  // namespace render
}  // namespace agea
