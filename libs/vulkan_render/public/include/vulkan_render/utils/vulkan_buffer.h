#pragma once

#include "vulkan_render/types/vulkan_generic.h"

#include <utils/defines_utils.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <functional>

namespace agea::render::vk_utils
{
class vulkan_buffer
{
public:
    vulkan_buffer();
    ~vulkan_buffer();

    static vulkan_buffer
    create(VkBufferCreateInfo bci, VmaAllocationCreateInfo vaci);

    AGEA_gen_class_non_copyable(vulkan_buffer);

    vulkan_buffer(vulkan_buffer&& other) noexcept;
    vulkan_buffer&
    operator=(vulkan_buffer&& other) noexcept;

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

    template <typename T>
    void
    upload_data(const T& t, bool alligment = true)
    {
        static_assert(!(std::is_reference<T>::value || std::is_pointer<T>::value));

        upload_data((std::uint8_t*)&t, (uint32_t)sizeof(T), alligment);
    }

    void
    upload_data(std::uint8_t* from, uint32_t size, bool alligment = true);

    std::uint8_t*
    allocate_data(uint32_t size);

    void
    begin();

    void
    end();

    void
    flush();

    VkDeviceSize
    get_offset() const
    {
        return m_offset;
    }

    VkDeviceSize
    get_alloc_size() const
    {
        return m_alloc_size;
    }

    VkDeviceSize
    get_range_alloc_size(uint32_t idx) const
    {
        auto size = (m_offsets.size() == (idx + 1)) ? m_offset : m_offsets[idx + 1];

        return size - m_offsets[idx];
    }

    uint32_t*
    get_dyn_offsets_ptr()
    {
        return m_offsets.empty() ? nullptr : m_offsets.data();
    }

    uint32_t
    get_dyn_offsets_count()
    {
        return (uint32_t)m_offsets.size();
    }

    const std::vector<uint32_t>&
    get_offsets() const
    {
        return m_offsets;
    }

    std::uint8_t*
    get_data()
    {
        return m_data_begin;
    }

private:
    vulkan_buffer(VkBuffer b, VmaAllocation a, VkDeviceSize alloc_size);

    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;

    uint32_t m_offset = 0U;
    VkDeviceSize m_alloc_size = 0U;
    std::uint8_t* m_data_begin = nullptr;

    std::vector<uint32_t> m_offsets;
};

}  // namespace agea::render::vk_utils
