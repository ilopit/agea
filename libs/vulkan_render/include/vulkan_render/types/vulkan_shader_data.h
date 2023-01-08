#pragma once

#include "vulkan_render/types/vulkan_types.h"

#include <utils/buffer.h>

namespace agea
{
namespace render
{
class shader_data
{
public:
    shader_data(vk_device_provider vk_device,
                VkShaderModule vk_module,
                ::agea::utils::buffer code,
                VkShaderStageFlagBits stage_bit);

    ~shader_data();

    VkShaderModule
    vk_module() const
    {
        return m_vk_module;
    }

    const ::agea::utils::buffer&
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
    vk_device_provider m_vk_device;
    VkShaderModule m_vk_module;
    ::agea::utils::buffer m_code;
};

}  // namespace render
}  // namespace agea
