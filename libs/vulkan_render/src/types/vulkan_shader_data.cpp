#include "vulkan_render/types/vulkan_shader_data.h"

namespace agea
{
namespace render
{
shader_data::shader_data(vk_device_provider vk_device,
                         VkShaderModule vk_module,
                         ::agea::utils::buffer code,
                         VkShaderStageFlagBits stage_bit)
    : m_vk_device(vk_device)
    , m_vk_module(vk_module)
    , m_code(code)
    , m_stage_bit(stage_bit)
{
}

shader_data::~shader_data()
{
    vkDestroyShaderModule(m_vk_device(), m_vk_module, nullptr);
}
}  // namespace render
}  // namespace agea