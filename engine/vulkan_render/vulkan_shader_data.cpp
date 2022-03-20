#include "vulkan_render/vulkan_shader_data.h"

namespace agea
{
namespace render
{
shader_data::~shader_data()
{
    vkDestroyShaderModule(m_vk_device, m_vk_module, nullptr);
}
}  // namespace render
}  // namespace agea