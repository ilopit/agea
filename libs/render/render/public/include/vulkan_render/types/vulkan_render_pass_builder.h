#pragma once

#include <vulkan_render/utils/vulkan_image.h>

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace kryga::render
{

class render_pass;
using render_pass_sptr = std::shared_ptr<render_pass>;

class render_pass_builder
{
public:
    render_pass_builder&
    set_color_format(VkFormat f);
    render_pass_builder&
    set_depth_format(VkFormat f);
    render_pass_builder&
    set_color_images(const std::vector<vk_utils::vulkan_image_view_sptr>& ivs,
                     const std::vector<vk_utils::vulkan_image_sptr>& is);
    render_pass_builder&
    set_width_depth(uint32_t w, uint32_t h);
    render_pass_builder&
    set_enable_stencil(bool stencil);
    render_pass_builder&
    set_depth_only(bool depth_only);
    render_pass_builder&
    set_image_count(uint32_t count);

    render_pass_sptr
    build();

private:
    std::vector<VkSubpassDependency>
    get_dependencies();
    std::vector<VkAttachmentDescription>
    get_attachments();

    VkFormat m_color_format = VK_FORMAT_UNDEFINED;
    VkFormat m_depth_format = VK_FORMAT_UNDEFINED;

    uint32_t m_width = 0U;
    uint32_t m_height = 0U;

    bool m_enable_stencil = true;
    bool m_depth_only = false;
    uint32_t m_image_count = 0;

    std::vector<vk_utils::vulkan_image_sptr> m_color_images;
    std::vector<vk_utils::vulkan_image_view_sptr> m_color_image_views;
};

}  // namespace kryga::render
