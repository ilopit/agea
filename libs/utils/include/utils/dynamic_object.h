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
    read_unsafe(uint32_t pos, uint8_t* data);

    uint8_t*
    data()
    {
        return m_data.data();
    }

    const uint8_t*
    data() const
    {
        return m_data.data();
    }

    uint32_t
    size() const
    {
        return (uint32_t)m_data.size();
    }

    const std::vector<uint8_t>&
    full_data()
    {
        return m_data;
    }

    uint32_t
    get_type_id(uint32_t pos);

private:
    std::vector<uint8_t> m_data;
    std::shared_ptr<dynamic_object_layout> m_layout;
};

}  // namespace utils
}  // namespace agea