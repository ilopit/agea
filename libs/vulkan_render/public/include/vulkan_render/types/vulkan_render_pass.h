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
    begin(VkCommandBuffer cmd,
          uint64_t swapchain_image_index,
          uint32_t width,
          uint32_t height,
          VkClearColorValue clear_color);

    bool
    end(VkCommandBuffer cmd);

    VkRenderPass
    vk()
    {
        return m_vk_render_pass;
    }

    std::vector<vk_utils::vulkan_image_sptr>
    get_color_images() const
    {
        return m_color_images;
    }

    std::vector<vk_utils::vulkan_image_view_sptr>
    get_color_image_views() const
    {
        return m_color_image_views;
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

    render_pass_builder&
    set_render_to_present(bool render_to_present)
    {
        m_render_to_present = render_to_present;

        return *this;
    }

    render_pass_builder&
    set_enable_stencil(bool stencil)
    {
        m_enable_stencil = stencil;

        return *this;
    }

    render_pass_sptr
    build();

private:
    VkFormat m_color_format = VK_FORMAT_UNDEFINED;
    VkFormat m_depth_format = VK_FORMAT_UNDEFINED;

    uint32_t m_width = 0U;
    uint32_t m_height = 0U;

    bool m_render_to_present = true;
    bool m_enable_stencil = true;

    std::vector<vk_utils::vulkan_image_sptr> m_color_images;
    std::vector<vk_utils::vulkan_image_view_sptr> m_color_image_views;
};

}  // namespace render
}  // namespace agea
