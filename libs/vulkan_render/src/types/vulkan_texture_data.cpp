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

texture_data::~texture_data()
{
    vkDestroyImageView(m_device(), image_view, nullptr);
}
}  // namespace render
}  // namespace agea
