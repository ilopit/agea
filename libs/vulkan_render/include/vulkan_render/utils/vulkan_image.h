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
