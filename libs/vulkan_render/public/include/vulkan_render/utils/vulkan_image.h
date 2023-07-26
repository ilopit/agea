#pragma once

#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_generic.h"

#include <utils/defines_utils.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <functional>

namespace agea::render::vk_utils
{
class vulkan_image
{
public:
    vulkan_image();
    ~vulkan_image();

    static vulkan_image
    create(vma_allocator_provider allocator,
           VkImageCreateInfo ici,
           VmaAllocationCreateInfo aci,
           int mips_level = 0);

    static vulkan_image create(VkImage);

    AGEA_gen_class_non_copyable(vulkan_image);

    vulkan_image(vulkan_image&& other) noexcept;
    vulkan_image&
    operator=(vulkan_image&& other) noexcept;

    void
    clear();

    VkImage
    image()
    {
        return m_image;
    }

    int
    get_mip_levels()
    {
        return mipLevels;
    }

private:
    vulkan_image(vma_allocator_provider a, int mips_level);

    VmaAllocation m_allocation;
    vma_allocator_provider m_allocator;
    VkImage m_image;
    int mipLevels = 1;
};

using vulkan_image_sptr = std::shared_ptr<vulkan_image>;

class vulkan_image_view;
using vulkan_image_view_sptr = std::shared_ptr<vulkan_image_view>;

class vulkan_image_view
{
public:
    vulkan_image_view() = default;
    ~vulkan_image_view();

    vulkan_image_view(vulkan_image_view&& other) noexcept;
    vulkan_image_view&
    operator=(vulkan_image_view&& other) noexcept;

    static vulkan_image_view
    create(const VkImageViewCreateInfo& iv_ci);

    static vulkan_image_view
    create(VkImageView&& vk_handle);

    static vulkan_image_view_sptr
    create_shared(const VkImageViewCreateInfo& iv_ci);

    static vulkan_image_view_sptr
    create_shared(VkImageView&& vk_handle);

    VkImageView
    vk() const
    {
        return m_vk_handle;
    }

    void
    clear();

private:
    vulkan_image_view(VkImageView vk_handle);

    VkImageView m_vk_handle = VK_NULL_HANDLE;
};

}  // namespace agea::render::vk_utils

#define VK_CHECK(x)                       \
    do                                    \
    {                                     \
        VkResult err = x;                 \
        if (err)                          \
        {                                 \
            AGEA_never("Vulkan failed!"); \
        }                                 \
    } while (0)
