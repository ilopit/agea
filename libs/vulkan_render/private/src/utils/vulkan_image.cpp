#include "vulkan_render/utils/vulkan_image.h"

#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/vulkan_render_device.h"

namespace agea::render::vk_utils
{

vulkan_image::vulkan_image(vma_allocator_provider a, int mips_level)
    : m_image()
    , m_allocator(a)
    , mipLevels(mips_level)
    , m_allocation(VK_NULL_HANDLE)
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
        glob::render_device::getr().delete_immidiately(
            [=](VkDevice vk, VmaAllocator va) { vmaDestroyImage(va, m_image, m_allocation); });
    }
    else
    {
        glob::render_device::getr().delete_immidiately([=](VkDevice vk, VmaAllocator va)
                                                       { vkDestroyImage(vk, m_image, nullptr); });
    }

    m_image = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
    m_allocator = nullptr;
}

std::uint8_t*
vulkan_image::map()
{
    vmaMapMemory(glob::render_device::getr().allocator(), m_allocation, (void**)&m_data_begin);

    return m_data_begin;
}

void
vulkan_image::unmap()
{
    vmaUnmapMemory(glob::render_device::getr().allocator(), m_allocation);
}

vulkan_image_view::~vulkan_image_view()
{
    clear();
}

vulkan_image_view::vulkan_image_view(vulkan_image_view&& other) noexcept
    : m_vk_handle(other.m_vk_handle)
{
    other.m_vk_handle = VK_NULL_HANDLE;
}

vulkan_image_view::vulkan_image_view(VkImageView vk_handle)
    : m_vk_handle(vk_handle)
{
}

vulkan_image_view&
vulkan_image_view::operator=(vulkan_image_view&& other) noexcept
{
    if (this != &other)
    {
        clear();

        m_vk_handle = other.m_vk_handle;
        other.m_vk_handle = VK_NULL_HANDLE;
    }

    return *this;
}

vulkan_image_view
vulkan_image_view::create(const VkImageViewCreateInfo& iv_ci)
{
    VkImageView iv = VK_NULL_HANDLE;

    vkCreateImageView(glob::render_device::getr().vk_device(), &iv_ci, nullptr, &iv);

    return vulkan_image_view(iv);
}

vulkan_image_view
vulkan_image_view::create(VkImageView&& vk_handle)
{
    vulkan_image_view iv(vk_handle);
    vk_handle = VK_NULL_HANDLE;

    return iv;
}

void
vulkan_image_view::clear()
{
    if (m_vk_handle)
    {
        glob::render_device::getr().delete_immidiately(
            [=](VkDevice vd, VmaAllocator) { vkDestroyImageView(vd, m_vk_handle, nullptr); });
    }
}

vulkan_image_view_sptr
vulkan_image_view::create_shared(const VkImageViewCreateInfo& iv_ci)
{
    return std::make_shared<vulkan_image_view>(vulkan_image_view::create(iv_ci));
}

vulkan_image_view_sptr
vulkan_image_view::create_shared(VkImageView&& vk_handle)
{
    return std::make_shared<vulkan_image_view>(vulkan_image_view::create(std::move(vk_handle)));
}

}  // namespace agea::render::vk_utils
