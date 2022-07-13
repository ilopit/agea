#include "vulkan_render_types/vulkan_shader_effect.h"

#include "vulkan_render/render_device.h"
#include "vulkan_render_types/vulkan_shader_data.h"

namespace agea
{
namespace render
{

void
shader_effect::add_stage(shader_data* shaderModule, VkShaderStageFlagBits stage)
{
    shader_stage newStage = {shaderModule, stage};
    m_stages.push_back(newStage);
}

}  // namespace render
}  // namespace agea
