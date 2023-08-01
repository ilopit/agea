#include "vulkan_render/utils/vulkan_buffer.h"

#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/vulkan_render_device.h"

namespace agea::render::vk_utils
{

vulkan_buffer&
vulkan_buffer::operator=(vulkan_buffer&& other) noexcept
{
    if (this != &other)
    {
        clear();

        m_buffer = other.m_buffer;
        other.m_buffer = VK_NULL_HANDLE;

        m_allocation = other.m_allocation;
        other.m_allocation = VK_NULL_HANDLE;

        m_offset = other.m_offset;
        other.m_offset = 0;

        m_alloc_size = other.m_alloc_size;
        other.m_alloc_size = 0;

        m_data_begin = other.m_data_begin;
        other.m_data_begin = nullptr;

        m_offsets = std::move(other.m_offsets);
    }

    return *this;
}

vulkan_buffer::vulkan_buffer()
    : m_buffer(VK_NULL_HANDLE)
    , m_allocation(VK_NULL_HANDLE)
    , m_alloc_size(0)
{
}

vulkan_buffer::vulkan_buffer(VkBuffer b, VmaAllocation a, VkDeviceSize alloc_size)
    : m_buffer(b)
    , m_allocation(a)
    , m_alloc_size(alloc_size)
{
}

vulkan_buffer::vulkan_buffer(vulkan_buffer&& other) noexcept
    : m_buffer(other.m_buffer)
    , m_allocation(other.m_allocation)
    , m_offset(other.m_offset)
    , m_data_begin(other.m_data_begin)
    , m_offsets(std::move(other.m_offsets))
    , m_alloc_size(other.m_alloc_size)
{
    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_offset = 0;
    other.m_data_begin = nullptr;
    other.m_alloc_size = 0;
}

void
vulkan_buffer::clear()
{
    glob::render_device::getr().delete_immidiately(
        [=](VkDevice vkd, VmaAllocator va) { vmaDestroyBuffer(va, m_buffer, m_allocation); });

    m_buffer = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
    m_alloc_size = 0;
}

vulkan_buffer
vulkan_buffer::create(VkBufferCreateInfo bci, VmaAllocationCreateInfo vaci)
{
    VkBuffer buffer;
    VmaAllocation allocation;

    vmaCreateBuffer(glob::render_device::getr().allocator(), &bci, &vaci, &buffer, &allocation,
                    nullptr);

    return vulkan_buffer{buffer, allocation, bci.size};
}

vulkan_buffer::~vulkan_buffer()
{
    clear();
}

void
vulkan_buffer::begin()
{
    m_offset = 0;
    m_offsets.clear();
    vmaMapMemory(glob::render_device::getr().allocator(), allocation(), (void**)&m_data_begin);
}

void
vulkan_buffer::end()
{
    vmaUnmapMemory(glob::render_device::getr().allocator(), allocation());
    m_data_begin = nullptr;
}

void
vulkan_buffer::flush()
{
    vmaFlushAllocation(glob::render_device::getr().allocator(), allocation(), 0, m_offset);
}

void
vulkan_buffer::upload_data(uint8_t* src, uint32_t size, bool use_alligment)
{
    auto device = glob::render_device::get();

    m_offsets.push_back(m_offset);

    auto data = m_data_begin + m_offset;
    memcpy(data, src, size);

    auto new_offset = m_offset + size;
    m_offset = use_alligment ? device->pad_uniform_buffer_size(new_offset) : new_offset;
}

uint8_t*
vulkan_buffer::allocate_data(uint32_t size)
{
    auto data = m_data_begin + m_offset;
    m_offsets.push_back(m_offset);
    m_offset += size;

    return data;
}

}  // namespace agea::render::vk_utils
