#include "vulkan_render_types/vulkan_types.h"

#include "vulkan_render_types/vk_initializers.h"

namespace agea
{
namespace render
{

allocated_buffer&
allocated_buffer::operator=(allocated_buffer&& other) noexcept
{
    if (this != &other)
    {
        clear();
    }
    m_buffer = other.m_buffer;
    m_allocation = other.m_allocation;
    m_allocator = other.m_allocator;

    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_allocator = nullptr;

    return *this;
}

allocated_buffer::allocated_buffer()
    : m_buffer(VK_NULL_HANDLE)
    , m_allocation(VK_NULL_HANDLE)
{
}

allocated_buffer::allocated_buffer(vma_allocator_provider alloc, VkBuffer b, VmaAllocation a)
    : m_buffer(b)
    , m_allocation(a)
    , m_allocator(alloc)
{
}

allocated_buffer::allocated_buffer(allocated_buffer&& other) noexcept
    : m_buffer(other.m_buffer)
    , m_allocation(other.m_allocation)
    , m_allocator(other.m_allocator)
{
    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_allocator = nullptr;
}

void
allocated_buffer::clear()
{
    if (!m_allocator)
    {
        return;
    }

    vmaDestroyBuffer(m_allocator(), m_buffer, m_allocation);

    m_buffer = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
    m_allocator = nullptr;
}

allocated_buffer
allocated_buffer::create(vma_allocator_provider alloc,
                         VkBufferCreateInfo bci,
                         VmaAllocationCreateInfo vaci)
{
    VkBuffer buffer;
    VmaAllocation allocation;

    vmaCreateBuffer(alloc(), &bci, &vaci, &buffer, &allocation, nullptr);

    return allocated_buffer{alloc, buffer, allocation};
}

allocated_buffer::~allocated_buffer()
{
    clear();
}

allocated_image::allocated_image(vma_allocator_provider a, int mips_level)
    : m_image()
    , m_allocator(a)
    , mipLevels(mips_level)
{
}

allocated_image::allocated_image(allocated_image&& other) noexcept
    : m_image(other.m_image)
    , m_allocation(other.m_allocation)
    , mipLevels(other.mipLevels)
    , m_allocator(other.m_allocator)
{
    other.m_image = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.mipLevels = 0;
    other.m_allocator = 0;
}

allocated_image::allocated_image()
    : m_image(VK_NULL_HANDLE)
    , m_allocation(VK_NULL_HANDLE)
    , mipLevels(1)
    , m_allocator()
{
}

allocated_image&
allocated_image::operator=(allocated_image&& other) noexcept
{
    if (this != &other)
    {
        clear();
    }
    m_image = other.m_image;
    m_allocation = other.m_allocation;
    mipLevels = other.mipLevels;
    m_allocator = other.m_allocator;

    other.m_image = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.mipLevels = 0;
    other.m_allocator = nullptr;

    return *this;
}

allocated_image::~allocated_image()
{
    clear();
}

allocated_image
allocated_image::create(vma_allocator_provider allocator,
                        VkImageCreateInfo ici,
                        VmaAllocationCreateInfo aci,
                        int mips_level)
{
    allocated_image new_image(allocator, mips_level);

    vmaCreateImage(allocator(), &ici, &aci, &new_image.m_image, &new_image.m_allocation, nullptr);

    return new_image;
}

void
allocated_image::clear()
{
    if (m_image == VK_NULL_HANDLE)
    {
        return;
    }

    vmaDestroyImage(m_allocator(), m_image, m_allocation);

    m_image = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
    m_allocator = nullptr;
}

}  // namespace render
}  // namespace agea
