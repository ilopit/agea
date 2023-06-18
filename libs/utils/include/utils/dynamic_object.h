#pragma once

#include "utils/id.h"
#include "utils/check.h"

#include <vector>
#include <string>
#include <memory>

namespace agea
{
namespace utils
{
class dynamic_object_layout;

class dynamic_object
{
public:
    dynamic_object() = default;

    dynamic_object(const std::shared_ptr<dynamic_object_layout>& l);

    dynamic_object(const std::shared_ptr<dynamic_object_layout>& l, size_t size);

    dynamic_object(const std::shared_ptr<dynamic_object_layout>& l, std::vector<uint8_t>* external);

    dynamic_object(const dynamic_object& other);

    dynamic_object&
    operator=(const dynamic_object& other);

    template <typename TYPE_DESCRIPTOR, typename T, typename... VARGS>
    bool
    write(uint32_t pos, const T& v, VARGS&&... args)
    {
        if (!write<TYPE_DESCRIPTOR>(pos, v))
        {
            return false;
        }

        return write<TYPE_DESCRIPTOR>(++pos, args...);
    }

    template <typename TYPE_DESCRIPTOR, typename T>
    bool
    write(uint32_t pos, const T& v)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(pos) != type_id)
        {
            return false;
        }

        write_unsafe(pos, (uint8_t*)&v);
        return true;
    }

    template <typename TYPE_DESCRIPTOR, typename... VARGS>
    bool
    write_obj(uint32_t field_idx, VARGS&&... args)
    {
        auto ojb_size = expected_size();

        if (m_cursor + ojb_size >= m_data->size())
        {
            m_data->insert(m_data->end(), m_data->size() - m_cursor + ojb_size, 0u);
        }

        bool r = write_fields<TYPE_DESCRIPTOR>((uint32_t)m_cursor, field_idx, args...);

        m_cursor += expected_size();

        return true;
    }

    template <typename TYPE_DESCRIPTOR, typename T, typename... VARGS>
    bool
    write_fields(uint32_t offset, uint32_t field_idx, const T& v, VARGS&&... args)
    {
        if (!write_fields<TYPE_DESCRIPTOR>(offset, field_idx, v))
        {
            return false;
        }

        return write_fields<TYPE_DESCRIPTOR>(offset, ++field_idx, args...);
        return true;
    }

    template <typename TYPE_DESCRIPTOR, typename T>
    bool
    write_fields(uint32_t offset, uint32_t field_idx, const T& v)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(field_idx) != type_id)
        {
            return false;
        }

        write_unsafe(offset, field_idx, (uint8_t*)&v);

        return true;
    }

    template <typename TYPE_DESCRIPTOR, typename T, typename... VARGS>
    bool
    read(uint32_t pos, T& v, VARGS&&... args)
    {
        if (!read<TYPE_DESCRIPTOR>(pos, v))
        {
            return false;
        }

        return read<TYPE_DESCRIPTOR>(++pos, args...);
    }

    template <typename TYPE_DESCRIPTOR, typename T>
    bool
    read(uint32_t pos, T& v)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(pos) != type_id)
        {
            return false;
        }

        read_unsafe(pos, (uint8_t*)&v);
        return true;
    }

    void
    write_unsafe(uint32_t pos, uint8_t* data);

    void
    write_unsafe(uint32_t offset, uint32_t field_idx, uint8_t* data);

    void
    read_unsafe(uint32_t pos, uint8_t* data);

    uint8_t*
    data()
    {
        return m_data->data();
    }

    const uint8_t*
    data() const
    {
        return m_data->data();
    }

    uint32_t
    size() const;

    uint32_t
    expected_size() const;

    uint32_t
    get_type_id(uint32_t pos);

private:
    uint64_t m_cursor = 0;

    std::vector<uint8_t> m_allocated;
    std::vector<uint8_t>* m_external_ref = nullptr;
    std::vector<uint8_t>* m_data = nullptr;

    std::shared_ptr<dynamic_object_layout> m_layout;
};

}  // namespace utils
}  // namespace agea