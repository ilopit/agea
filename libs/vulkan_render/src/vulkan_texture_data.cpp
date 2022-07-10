#include "vulkan_render/vulkan_texture_data.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "vulkan_render/render_device.h"

namespace agea
{
namespace render
{
texture_data::~texture_data()
{
    auto device = glob::render_device::get();
    vkDestroyImageView(device->vk_device(), image_view, nullptr);
}
}  // namespace render
}  // namespace agea
