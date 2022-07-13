// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vulkan_render_types/vulkan_types.h"

#include <vector>
#include <array>
#include <unordered_map>
#include <memory>

namespace agea
{
namespace render
{

struct shader_data
{
    shader_data(vk_device_provider vk_device, VkShaderModule vk_module, std::vector<char> code)
        : m_vk_device(vk_device)
        , m_vk_module(vk_module)
        , m_code(std::move(code))
    {
    }

    ~shader_data();

    VkShaderModule
    vk_module()
    {
        return m_vk_module;
    }

    const std::vector<char>&
    code()
    {
        return m_code;
    }

private:
    vk_device_provider m_vk_device;
    VkShaderModule m_vk_module;
    std::vector<char> m_code;
};

}  // namespace render
}  // namespace agea
