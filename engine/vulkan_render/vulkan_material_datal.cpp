#include "vulkan_material_data.h"

#include "render_device.h"

namespace agea
{
namespace render
{
material_data::~material_data()
{
    vkDestroySampler(glob::render_device::get()->vk_device(), sampler, nullptr);
}
}  // namespace render
}  // namespace agea