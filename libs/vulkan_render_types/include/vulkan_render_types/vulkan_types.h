#pragma once

#include "vulkan_render_types/vulkan_gpu_types.h"

#include "vulkan_render_types/vulkan_generic.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <functional>

namespace agea
{
namespace render
{

class allocated_buffer
{
public:
    allocated_buffer();

    ~allocated_buffer();

    allocated_buffer(vma_allocator_provider alloc, VkBuffer b, VmaAllocation a);

    allocated_buffer(const allocated_buffer&) = delete;
    allocated_buffer&
    operator=(const allocated_buffer&) = delete;

    allocated_buffer(allocated_buffer&& other) noexcept;
    allocated_buffer&
    operator=(allocated_buffer&& other) noexcept;

    static allocated_buffer
    create(vma_allocator_provider alloc, VkBufferCreateInfo bci, VmaAllocationCreateInfo vaci);

    void
    clear();

    VkBuffer&
    buffer()
    {
        return m_buffer;
    }

    VmaAllocation
    allocation()
    {
        return m_allocation;
    }

    VmaAllocator
    allocator()
    {
        return m_allocator();
    }

private:
    VkBuffer m_buffer;
    VmaAllocation m_allocation;
    vma_allocator_provider m_allocator;
};

class allocated_image
{
public:
    allocated_image();
    ~allocated_image();

    allocated_image(const allocated_image&) = delete;
    allocated_image&
    operator=(const allocated_image&) = delete;

    allocated_image(allocated_image&& other) noexcept;
    allocated_image&
    operator=(allocated_image&& other) noexcept;

    static allocated_image
    create(vma_allocator_provider allocator,
           VkImageCreateInfo ici,
           VmaAllocationCreateInfo aci,
           int mips_level = 0);

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
    allocated_image(vma_allocator_provider a, int mips_level);

    VkImage m_image;
    VmaAllocation m_allocation;
    int mipLevels = 1;
    vma_allocator_provider m_allocator;
};

}  // namespace render
}  // namespace agea

#define VK_CHECK(x)                       \
    do                                    \
    {                                     \
        VkResult err = x;                 \
        if (err)                          \
        {                                 \
            AGEA_never("Vulkan failed!"); \
        }                                 \
    } while (0)
