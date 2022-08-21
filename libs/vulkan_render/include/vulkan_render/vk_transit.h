#pragma once

#include <vulkan_render_types/vulkan_types.h>

#include <vk_mem_alloc.h>

#include <vector>
#include <stdint.h>
#include <utils/check.h>

namespace agea
{
namespace render
{

class transit_buffer : public allocated_buffer
{
public:
    transit_buffer()
    {
    }

    transit_buffer(vma_allocator_provider alloc, VkBuffer b, VmaAllocation a)
        : allocated_buffer(alloc, b, a)
    {
    }

    transit_buffer(allocated_buffer alloc)
        : allocated_buffer(std::move(alloc))
    {
    }

    explicit transit_buffer(transit_buffer&& other) noexcept
        : allocated_buffer(std::move(other))
        , m_offset(other.m_offset)
        , m_data_begin(other.m_data_begin)
        , m_offsets(std::move(other.m_offsets))
    {
        other.m_offset = 0;
        other.m_data_begin = nullptr;
    }

    transit_buffer&
    operator=(transit_buffer&& other) noexcept
    {
        if (this != &other)
        {
            allocated_buffer::operator=(std::move(other));

            m_offset = other.m_offset;
            other.m_offset = 0;

            m_data_begin = other.m_data_begin;
            other.m_data_begin = nullptr;

            m_offsets = std::move(other.m_offsets);
        }
        return *this;
    }

    ~transit_buffer()
    {
        AGEA_check(!m_data_begin, "End should be called");
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

    uint32_t
    get_offset()
    {
        return m_offset;
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

private:
    uint32_t m_offset = 0U;
    std::uint8_t* m_data_begin = nullptr;

    std::vector<uint32_t> m_offsets;
};
}  // namespace render
}  // namespace agea
