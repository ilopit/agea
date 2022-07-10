// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vulkan_types.h"
#include "vulkan_render/vk_descriptors.h"

#include <vector>
#include <array>
#include <unordered_map>
#include <memory>

namespace agea
{
namespace render
{
class render_device;

struct shader_data
{
    shader_data(VkDevice vk_device, VkShaderModule vk_module, std::vector<char> code)
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
    VkDevice m_vk_device;
    VkShaderModule m_vk_module;
    std::vector<char> m_code;
};

}  // namespace render
}  // namespace agea
