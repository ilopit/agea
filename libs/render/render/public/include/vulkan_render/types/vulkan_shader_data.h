#pragma once

#include "vulkan_render/types/vulkan_generic.h"

#include <utils/buffer.h>

namespace kryga
{
namespace render
{
class shader_module_data
{
public:
    shader_module_data(VkShaderModule vk_module,
                       ::kryga::utils::buffer code,
                       VkShaderStageFlagBits stage_bit);

    ~shader_module_data();

    VkShaderModule
    vk_module() const
    {
        return m_vk_module;
    }

    const ::kryga::utils::buffer&
    code() const
    {
        return m_code;
    }

    VkShaderStageFlagBits
    stage() const
    {
        return m_stage_bit;
    }

private:
    VkShaderStageFlagBits m_stage_bit;
    VkShaderModule m_vk_module;
    ::kryga::utils::buffer m_code;
};

}  // namespace render
}  // namespace kryga
