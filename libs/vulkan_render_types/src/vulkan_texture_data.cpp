#include "vulkan_render_types/vulkan_texture_data.h"

namespace agea
{
namespace render
{
texture_data::~texture_data()
{
    vkDestroyImageView(m_device(), image_view, nullptr);
}
}  // namespace render
}  // namespace agea
