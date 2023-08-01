#include "vulkan_render/types/vulkan_shader_data.h"

#include "vulkan_render/vulkan_render_device.h"

namespace agea
{
namespace render
{
shader_module_data::shader_module_data(VkShaderModule vk_module,
                                       ::agea::utils::buffer code,
                                       VkShaderStageFlagBits stage_bit)
    : m_vk_module(vk_module)
    , m_code(code)
    , m_stage_bit(stage_bit)
{
}

shader_module_data::~shader_module_data()
{
    glob::render_device::getr().delete_immidiately(
        [=](VkDevice vkd, VmaAllocator) { vkDestroyShaderModule(vkd, m_vk_module, nullptr); });
}
}  // namespace render
}  // namespace agea