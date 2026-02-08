#include "vulkan_render/types/vulkan_shader_data.h"

#include "vulkan_render/vulkan_render_device.h"

#include <global_state/global_state.h>

namespace kryga
{
namespace render
{
shader_module_data::shader_module_data(VkShaderModule vk_module,
                                       ::kryga::utils::buffer code,
                                       VkShaderStageFlagBits stage_bit,
                                       reflection::shader_reflection reflection)
    : m_vk_module(vk_module)
    , m_code(std::move(code))
    , m_stage_bit(stage_bit)
    , m_reflection(std::move(reflection))
{
}

shader_module_data::~shader_module_data()
{
    glob::glob_state().getr_render_device().delete_immediately(
        [=](VkDevice vkd, VmaAllocator) { vkDestroyShaderModule(vkd, m_vk_module, nullptr); });
}
}  // namespace render
}  // namespace kryga