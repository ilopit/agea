#pragma once

#include <vulkan_render/types/vulkan_gpu_types.h>

#include <utils/id.h>
#include <utils/id_allocator.h>

namespace agea
{
namespace render
{

template <typename T>
struct buffer_segment
{
    gpu_data_index_type
    alloc_id()
    {
        auto id = id_alloc.alloc_id();

        return id;
    }

    uint64_t
    get_used_size() const
    {
        auto current_size = id_alloc.get_ids_in_fly() * type_size;

        return current_size;
    }

    uint64_t
    get_allocated_size() const
    {
        return reserved_size;
    }

    void
    set_allocated_size(uint64_t v)
    {
        reserved_size = v;
    }

    utils::id name;
    utils::id_allocator id_alloc;

    gpu_data_index_type index = INVALID_GPU_INDEX;
    uint64_t type_size = 0;
    uint64_t reserved_size = 0;
    uint64_t offset = 0;

    std::vector<T> selected;
    bool in_usage = false;
    bool out_of_range = false;
};

template <typename T>
struct buffer_layout
{
    buffer_segment<T>*
    find(const utils::id& type_id)
    {
        for (auto& s : m_segments)
        {
            if (s.name == type_id)
            {
                return &s;
            }
        }

        return nullptr;
    }

    buffer_segment<T>*
    add(const utils::id& name, uint64_t type_size, uint64_t reserved_size)
    {
        AGEA_check(!find(name), "Should not exists !");

        auto new_id = m_id_alloc.alloc_id();

        buffer_segment<T>* result = nullptr;

        if (new_id >= m_segments.size())
        {
            m_segments.resize(new_id + 1);

            result = &m_segments[new_id];

            AGEA_check(!result->in_usage, "Should not exists !");

            result->offset = calc_offset(new_id);
            result->reserved_size = reserved_size;
            result->index = new_id;
        }
        else
        {
            result = &m_segments[new_id];

            AGEA_check(!result->in_usage, "Should not exists !");
        }

        result->in_usage = true;
        result->type_size = type_size;
        result->name = name;

        return result;
    }

    buffer_segment<T>&
    at(uint64_t i)
    {
        return m_segments[i];
    }

    uint64_t
    size()
    {
        uint64_t result = 0;
        for (auto& s : m_segments)
        {
            result += s.get_allocated_size();
        }

        return result;
    }

    uint64_t
    calc_new_size()
    {
        uint64_t result = 0;
        for (auto& s : m_segments)
        {
            if (s.get_used_size() > s.get_allocated_size())
            {
                result += s.get_used_size();
                m_changed_layout = true;
            }
            else
            {
                result += s.get_allocated_size();
            }
        }

        return result;
    }

    void
    update_segment(gpu_data_index_type id)
    {
        auto& s = m_segments[id];

        if (s.get_used_size() >= s.get_allocated_size())
        {
            s.set_allocated_size(s.get_used_size() * 2);
            m_changed_layout = true;
        }

        if (m_changed_layout)
        {
            s.offset = calc_offset(s.index);

            if (m_segments.size() == (id + 1))
            {
                m_changed_layout = false;
            }
        }
    }

    uint64_t
    get_segments_size() const
    {
        return m_segments.size();
    }

    bool
    dirty_layout()
    {
        return m_changed_layout;
    }

    void
    reset_dirty_layout()
    {
        m_changed_layout = false;
    }

private:
    uint64_t
    calc_offset(gpu_data_index_type id)
    {
        return id ? (m_segments[id - 1].offset + m_segments[id - 1].get_allocated_size()) : 0U;
    }

    std::vector<buffer_segment<T>> m_segments;
    utils::id_allocator m_id_alloc;
    bool m_changed_layout = false;
};

}  // namespace render
}  // namespace agea