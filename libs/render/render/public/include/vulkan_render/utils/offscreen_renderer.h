#pragma once

#include <vulkan_render/utils/vulkan_buffer.h>
#include <vulkan_render/utils/vulkan_image.h>

#include <gpu_types/gpu_camera_types.h>
#include <gpu_types/gpu_generic_constants.h>
#include <gpu_types/gpu_light_types.h>
#include <gpu_types/gpu_object_types.h>

#include <cstdint>
#include <vector>

namespace kryga::render
{

class render_device;
class render_pass;
class shader_effect_data;
class mesh_data;

struct offscreen_draw_request
{
    mesh_data* mesh = nullptr;
    shader_effect_data* shader_effect = nullptr;

    gpu::camera_data camera{};
    gpu::object_data object{};
    gpu::directional_light_data directional_light{};

    const void* material_gpu_data = nullptr;
    size_t material_gpu_data_size = 0;

    uint32_t texture_indices[KGPU_MAX_TEXTURE_SLOTS] = {};
    uint32_t sampler_indices[KGPU_MAX_TEXTURE_SLOTS] = {};

    VkDescriptorSet bindless_set = VK_NULL_HANDLE;

    float clear_color[4] = {0.12f, 0.12f, 0.14f, 1.0f};
};

struct offscreen_render_result
{
    std::vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
};

class offscreen_renderer
{
public:
    offscreen_renderer() = default;

    void
    init(render_device& device, render_pass* pass, uint32_t size);
    void
    destroy(VkDevice device);

    offscreen_render_result
    render(render_device& device, const offscreen_draw_request& request);

    bool
    is_initialized() const
    {
        return m_initialized;
    }
    uint32_t
    size() const
    {
        return m_size;
    }

private:
    bool m_initialized = false;
    uint32_t m_size = 0;

    VkRenderPass m_vk_pass = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
    VkFormat m_color_format = VK_FORMAT_UNDEFINED;

    vk_utils::vulkan_image_sptr m_color_image;
    vk_utils::vulkan_image_view_sptr m_color_view;
    vk_utils::vulkan_image m_depth_image;
    vk_utils::vulkan_image_view_sptr m_depth_view;

    vk_utils::vulkan_buffer m_camera_buf;
    vk_utils::vulkan_buffer m_objects_buf;
    vk_utils::vulkan_buffer m_slots_buf;
    vk_utils::vulkan_buffer m_dir_lights_buf;
    vk_utils::vulkan_buffer m_material_buf;
    vk_utils::vulkan_buffer m_dummy_buf;
};

}  // namespace kryga::render
