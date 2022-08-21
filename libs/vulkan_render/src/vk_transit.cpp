#pragma once

#include "vulkan_render/vk_transit.h"

#include "vulkan_render/render_device.h"

namespace agea
{
namespace render
{

void
transit_buffer::begin()
{
    m_offset = 0;
    m_offsets.clear();
    vmaMapMemory(allocator(), allocation(), (void**)&m_data_begin);
}

void
transit_buffer::end()
{
    vmaUnmapMemory(allocator(), allocation());
    m_data_begin = nullptr;
}

void
transit_buffer::upload_data(uint8_t* src, uint32_t size, bool use_alligment)
{
    auto device = glob::render_device::get();

    m_offsets.push_back(m_offset);

    auto data = m_data_begin + m_offset;
    memcpy(data, src, size);

    auto new_offset = m_offset + size;
    m_offset = use_alligment ? device->pad_uniform_buffer_size(new_offset) : new_offset;
}

uint8_t*
transit_buffer::allocate_data(uint32_t size)
{
    auto data = m_data_begin + m_offset;
    m_offsets.push_back(m_offset);
    m_offset += size;

    return data;
}

}  // namespace render
}  // namespace agea