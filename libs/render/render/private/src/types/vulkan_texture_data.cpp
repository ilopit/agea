#include "vulkan_render/types/vulkan_texture_data.h"

namespace kryga
{
namespace render
{

texture_data::texture_data(const ::kryga::utils::id& id)
    : m_id(id)
{
}

texture_data::texture_data(texture_data&& other) noexcept
    : image_view(other.image_view)
    , image(std::move(other.image))
    , format(other.format)
    , m_id(std::move(other.m_id))
    , m_bindless_index(other.m_bindless_index)
{
    other.format = texture_format::unknown;
    other.m_bindless_index = INVALID_BINDLESS_INDEX;
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

        m_bindless_index = other.m_bindless_index;
        other.m_bindless_index = INVALID_BINDLESS_INDEX;
    }

    return *this;
}

texture_data::~texture_data()
{
}
}  // namespace render
}  // namespace kryga
