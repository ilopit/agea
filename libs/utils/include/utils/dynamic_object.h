#pragma once

#include "utils/id.h"
#include "utils/agea_types.h"
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

    template <typename TYPE_DESCRIPTOR, typename T, typename... VARGS>
    bool
    write_at(uint32_t obj_idx, uint32_t field_idx, const T& v, VARGS&&... args)
    {
        if (!write_at<TYPE_DESCRIPTOR>(obj_idx, field_idx, v))
        {
            return false;
        }

        return write_at<TYPE_DESCRIPTOR>(obj_idx, ++field_idx, args...);
    }

    template <typename TYPE_DESCRIPTOR, typename T>
    bool
    write_at(uint32_t obj_idx, uint32_t field_idx, const T& v)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(field_idx) != type_id)
        {
            return false;
        }

        write_unsafe(obj_idx * expected_size(), field_idx, (uint8_t*)&v);
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
        return m_external_data ? m_external_data : m_data.data();
    }

    const uint8_t*
    data() const
    {
        return data();
    }

    uint32_t
    size() const;

    uint32_t
    expected_size() const;

    const std::vector<uint8_t>&
    full_data()
    {
        return m_data;
    }

    void
    set_external(uint8_t* ptr)
    {
        m_external_data = ptr;
    }

    uint32_t
    get_type_id(uint32_t pos);

private:
    std::vector<uint8_t> m_data;
    uint8_t* m_external_data = nullptr;
    std::shared_ptr<dynamic_object_layout> m_layout;
};

}  // namespace utils
}  // namespace agea