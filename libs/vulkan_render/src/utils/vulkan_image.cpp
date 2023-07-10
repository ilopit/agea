#include "vulkan_render/utils/vulkan_image.h"

#include "vulkan_render/utils/vulkan_initializers.h"

namespace agea::render::vk_utils
{

vulkan_image::vulkan_image(vma_allocator_provider a, int mips_level)
    : m_image()
    , m_allocator(a)
    , mipLevels(mips_level)
{
}

vulkan_image::vulkan_image(vulkan_image&& other) noexcept
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

vulkan_image::vulkan_image()
    : m_image(VK_NULL_HANDLE)
    , m_allocation(VK_NULL_HANDLE)
    , mipLevels(1)
    , m_allocator()
{
}

vulkan_image&
vulkan_image::operator=(vulkan_image&& other) noexcept
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

vulkan_image::~vulkan_image()
{
    clear();
}

vulkan_image
vulkan_image::create(vma_allocator_provider allocator,
                     VkImageCreateInfo ici,
                     VmaAllocationCreateInfo aci,
                     int mips_level)
{
    vulkan_image new_image(allocator, mips_level);

    vmaCreateImage(allocator(), &ici, &aci, &new_image.m_image, &new_image.m_allocation, nullptr);

    return new_image;
}

vulkan_image
vulkan_image::create(VkImage image)
{
    vulkan_image new_image(nullptr, 0);

    new_image.m_image = image;

    return new_image;
}

void
vulkan_image::clear()
{
    if (m_image == VK_NULL_HANDLE)
    {
        return;
    }

    if (m_allocator)
    {
        vmaDestroyImage(m_allocator(), m_image, m_allocation);
    }
    else
    {
        // TODO
    }

    m_image = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
    m_allocator = nullptr;
}

}  // namespace agea::render::vk_utils
