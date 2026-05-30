#include "vulkan_render/utils/vulkan_buffer.h"

#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/utils/vulkan_debug.h"
#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/render_system.h"

#include <global_state/global_state.h>

namespace kryga::render::vk_utils
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
    if (m_buffer != VK_NULL_HANDLE)
    {
        glob::glob_state().getr_render().device.schedule_to_delete(
            [buf = m_buffer, alloc = m_allocation](VkDevice, VmaAllocator va)
            { vmaDestroyBuffer(va, buf, alloc); });
    }

    m_buffer = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
    m_alloc_size = 0;
}

void
vulkan_buffer::clone_contents_from(vulkan_buffer& src, VkBufferUsageFlags usage)
{
    if (src.m_buffer == VK_NULL_HANDLE)
    {
        return;
    }

    if (m_alloc_size < src.m_alloc_size)
    {
        *this = glob::glob_state().getr_render().device.create_buffer(
            src.m_alloc_size, usage, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    src.begin();
    begin();

    memcpy(m_data_begin, src.m_data_begin, src.m_alloc_size);

    end();
    src.end();
}

vulkan_buffer
vulkan_buffer::create(VkBufferCreateInfo bci,
                      VmaAllocationCreateInfo vaci,
                      std::string_view debug_name)
{
    VkBuffer buffer;
    VmaAllocation allocation;

    auto& device = glob::glob_state().getr_render().device;

    VK_CHECK(vmaCreateBuffer(device.allocator(), &bci, &vaci, &buffer, &allocation, nullptr));

    KRG_VK_NAME(device.vk_device(), buffer, debug_name);

    return vulkan_buffer{buffer, allocation, bci.size};
}

VkDeviceAddress
vulkan_buffer::device_address() const
{
    VkBufferDeviceAddressInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = m_buffer;

    auto device = glob::glob_state().getr_render().device.vk_device();

#if defined(__ANDROID__)
    // Android's libvulkan.so only exports Vulkan 1.0 symbols. Load the 1.2 core
    // function pointer dynamically on first use and cache it.
    static PFN_vkGetBufferDeviceAddress pfn = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(
        vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddress"));
    return pfn(device, &info);
#else
    return vkGetBufferDeviceAddress(device, &info);
#endif
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
    vmaMapMemory(
        glob::glob_state().getr_render().device.allocator(), allocation(), (void**)&m_data_begin);
}

void
vulkan_buffer::end()
{
    vmaUnmapMemory(glob::glob_state().getr_render().device.allocator(), allocation());
    m_data_begin = nullptr;
}

void
vulkan_buffer::flush()
{
    vmaFlushAllocation(
        glob::glob_state().getr_render().device.allocator(), allocation(), 0, m_offset);
}

void
vulkan_buffer::upload_data(uint8_t* src, uint32_t size, bool use_alignment)
{
    auto& device = glob::glob_state().getr_render().device;

    m_offsets.push_back(m_offset);

    auto data = m_data_begin + m_offset;
    memcpy(data, src, size);

    auto new_offset = m_offset + size;
    m_offset = use_alignment ? device.pad_uniform_buffer_size(new_offset) : new_offset;
}

uint8_t*
vulkan_buffer::allocate_data(uint32_t size)
{
    auto data = m_data_begin + m_offset;
    m_offsets.push_back(m_offset);
    m_offset += size;

    return data;
}

}  // namespace kryga::render::vk_utils
