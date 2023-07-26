#pragma once

#include <vulkan_render/types/vulkan_generic.h>

#include <vulkan_render/utils/vulkan_image.h>

namespace agea
{
namespace render
{
class render_pass
{
public:
    friend class render_pass_builder;

    render_pass() = default;
    ~render_pass();

    AGEA_gen_class_non_copyable(render_pass);

    bool
    begin(VkCommandBuffer cmd, uint64_t swapchain_image_index, uint32_t width, uint32_t height);

    bool
    end(VkCommandBuffer cmd);

    VkRenderPass
    vk()
    {
        return m_vk_render_pass;
    }

private:
    std::vector<VkFramebuffer> m_framebuffers;

    std::vector<vk_utils::vulkan_image_sptr> m_color_images;
    std::vector<vk_utils::vulkan_image_view_sptr> m_color_image_views;

    std::vector<vk_utils::vulkan_image_view> m_depth_image_views;
    std::vector<vk_utils::vulkan_image> m_depth_images;

    VkRenderPass m_vk_render_pass = VK_NULL_HANDLE;
};

using render_pass_sptr = std::shared_ptr<render_pass>;

class render_pass_builder
{
public:
    render_pass_builder&
    set_color_format(VkFormat f)
    {
        m_color_format = f;
        return *this;
    }

    render_pass_builder&
    set_depth_format(VkFormat f)
    {
        m_depth_format = f;
        return *this;
    }

    render_pass_builder&
    set_color_images(const std::vector<vk_utils::vulkan_image_view_sptr>& ivs,
                     const std::vector<vk_utils::vulkan_image_sptr>& is);

    render_pass_builder&
    set_width_depth(uint32_t w, uint32_t h)
    {
        m_width = w;
        m_height = h;

        return *this;
    }

    render_pass_sptr
    build();

private:
    VkFormat m_color_format;
    VkFormat m_depth_format;

    uint32_t m_width;
    uint32_t m_height;

    std::vector<vk_utils::vulkan_image_sptr> m_color_images;
    std::vector<vk_utils::vulkan_image_view_sptr> m_color_image_views;
};

}  // namespace render
}  // namespace agea
