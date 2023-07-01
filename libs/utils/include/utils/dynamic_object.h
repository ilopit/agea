#pragma once

#include "utils/id.h"
#include "utils/check.h"
#include "utils/math_utils.h"

#include <vector>
#include <string>
#include <memory>

namespace agea
{
namespace utils
{
class dynobj_layout;
struct dynobj_field;

class base_view
{
public:
    base_view() = default;

    bool
    is_array() const;

    bool
    is_object() const;

    bool
    valid() const;

    uint64_t
    size(uint64_t idx) const;

    uint64_t
    offset(uint64_t idx) const;

    static const dynobj_field*
    dyn_field(const std::shared_ptr<dynobj_layout>& layout, uint64_t idx);

protected:
    base_view(uint64_t offset,
              uint8_t* data,
              const dynobj_field* cur_field,
              const std::shared_ptr<dynobj_layout>& layout);

    static uint32_t
    get_type_id(const dynobj_field* f);

    static bool
    is_array(const dynobj_field* f);

    static bool
    is_object(const dynobj_field* f);

    static uint64_t
    get_idx_offset(const dynobj_field* f, uint64_t idx);

    bool
    write_unsafe(const dynobj_field* f, uint64_t offset, uint8_t* src);

    bool
    read_unsafe(const dynobj_field* f, uint64_t offset, uint8_t* dst);

    uint64_t
    get_offest(uint64_t idx);

    base_view
    build_for_subobject(uint64_t field_idx);

    base_view
    build_for_subobject(uint64_t field_idx, uint64_t idx);

    const dynobj_field*
    sub_field_by_idx(uint64_t idx) const;

    uint64_t m_offset = 0;
    uint8_t* m_data = nullptr;
    const dynobj_field* m_cur_field = nullptr;
    std::shared_ptr<dynobj_layout> m_layout = nullptr;
};

template <typename TYPE_DESCRIPTOR>
class dynobj_view : public base_view
{
public:
    dynobj_view() = default;

    dynobj_view(uint32_t offset,
                uint8_t* data,
                const dynobj_field* cur_field,
                std::shared_ptr<dynobj_layout> layout)
        : base_view(offset, data, cur_field, layout)
    {
    }

    dynobj_view(const base_view& bf)
        : base_view(bf)
    {
    }

    dynobj_view
    subobj(uint32_t field_idx)
    {
        AGEA_check(is_object(), "Should be object!");

        dynobj_view<TYPE_DESCRIPTOR> f(build_for_subobject(field_idx));

        return f;
    }

    dynobj_view
    subobj(uint32_t field_idx, uint32_t idx)
    {
        AGEA_check(is_object(), "Should be an object!");

        dynobj_view<TYPE_DESCRIPTOR> f(build_for_subobject(field_idx, idx));

        return f;
    }

    template <typename T, typename... VARGS>
    bool
    write_from(uint32_t field_idx, const T& v, VARGS&&... args)
    {
        AGEA_check(is_object(), "Should be object!");

        auto f = sub_field_by_idx(field_idx);

        if (!f)
        {
            return false;
        }

        if (!write_impl(f, v))
        {
            return false;
        }

        return write_from(++field_idx, args...);
    }

    template <typename T, typename... VARGS>
    bool
    write(const T& v, VARGS&&... args)
    {
        AGEA_check(is_object(), "Should be object!");

        auto f = sub_field_by_idx(0);

        if (!f)
        {
            return false;
        }

        if (!write_impl(f, v))
        {
            return false;
        }

        return write_from(1, args...);
    }

    template <typename T, typename... VARGS>
    bool
    write_from(uint32_t field_idx, const T& v)
    {
        AGEA_check(is_object(), "Should be object!");

        auto f = sub_field_by_idx(field_idx);

        if (!f)
        {
            return false;
        }

        return write_impl(f, v);
    }

    template <typename T, typename... VARGS>
    bool
    read(uint32_t field_idx, const T& v, VARGS&&... args)
    {
        auto f = sub_field_by_idx(field_idx);

        AGEA_check(!is_array(f), "Should not be array!");

        if (!f || !read_impl(f, v))
        {
            return false;
        }

        return read(++field_idx, args...);
    }

    template <typename T, typename... VARGS>
    bool
    read(uint32_t field_idx, T& v)
    {
        auto f = sub_field_by_idx(field_idx);

        AGEA_check(!is_array(f), "Should not be array!");

        if (!f)
        {
            return false;
        }

        return read_impl(f, v);
    }

    template <typename... VARGS>
    bool
    write_array(uint32_t field_idx, uint32_t idx, VARGS&&... args)
    {
        AGEA_check(is_object(), "Should be object!");

        auto f = sub_field_by_idx(field_idx);

        if (!f)
        {
            return false;
        }

        AGEA_check(is_array(f), "Should be array!");

        return write_array_impl(f, idx, args...);
    }

    template <typename... VARGS>
    bool
    read_array(uint32_t field_idx, uint32_t idx, VARGS&&... args)
    {
        AGEA_check(is_object(), "Should be object!");

        auto f = sub_field_by_idx(field_idx);

        if (!f)
        {
            return false;
        }

        AGEA_check(is_array(f), "Should be object!");

        return read_array_impl(f, idx, args...);
    }

private:
    template <typename T>
    bool
    write_impl(const dynobj_field* dyn_field, const T& v)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(dyn_field) != type_id)
        {
            return false;
        }

        return write_unsafe(dyn_field, m_offset, (uint8_t*)&v);
    }

    template <typename T, typename... VARGS>
    bool
    write_array_impl(const dynobj_field* dyn_field, uint32_t idx, const T& v, VARGS&&... args)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(dyn_field) != type_id)
        {
            return false;
        }

        auto offset = get_idx_offset(dyn_field, idx) + m_offset;

        if (!write_unsafe(dyn_field, offset, (uint8_t*)&v))
        {
            return false;
        }

        return write_array_impl(dyn_field, ++idx, args...);
    }

    template <typename T>
    bool
    write_array_impl(const dynobj_field* dyn_field, uint32_t idx, const T& v)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(dyn_field) != type_id)
        {
            return false;
        }

        auto offset = get_idx_offset(dyn_field, idx) + m_offset;

        return write_unsafe(dyn_field, offset, (uint8_t*)&v);
    }

    template <typename T>
    bool
    read_impl(const dynobj_field* dyn_field, T& v)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(dyn_field) != type_id)
        {
            return false;
        }

        return read_unsafe(dyn_field, m_offset, (uint8_t*)&v);
    }

    template <typename T, typename... VARGS>
    bool
    read_array_impl(const dynobj_field* dyn_field, uint32_t idx, const T& v, VARGS&&... args)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(dyn_field) != type_id)
        {
            return false;
        }

        auto offset = get_idx_offset(dyn_field, idx) + m_offset;

        if (!read_unsafe(dyn_field, offset, (uint8_t*)&v))
        {
            return false;
        }

        return read_array_impl(dyn_field, ++idx, args...);
    }

    template <typename T>
    bool
    read_array_impl(const dynobj_field* dyn_field, uint32_t idx, const T& v)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(dyn_field) != type_id)
        {
            return false;
        }

        auto offset = get_idx_offset(dyn_field, idx) + m_offset;

        return read_unsafe(dyn_field, offset, (uint8_t*)&v);
    }
};

class dynobj
{
public:
    dynobj()
        : m_layout()
    {
    }

    dynobj(const std::shared_ptr<dynobj_layout>& l);

    uint8_t*
    data()
    {
        return m_obj_data.data();
    }

    const uint8_t*
    data() const
    {
        return m_obj_data.data();
    }

    uint64_t
    size() const
    {
        return m_obj_data.size();
    }

    template <typename TYPE_DESCRIPTOR>
    dynobj_view<TYPE_DESCRIPTOR>
    root()
    {
        return dynobj_view<TYPE_DESCRIPTOR>(0, data(), base_view::dyn_field(m_layout, 0), m_layout);
    }

private:
    const dynobj_field*
    get_dyn_field(uint64_t idx);

    std::vector<uint8_t> m_obj_data;
    std::shared_ptr<dynobj_layout> m_layout;
};

}  // namespace utils
}  // namespace agea