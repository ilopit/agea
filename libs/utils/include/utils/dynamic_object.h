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
struct dynobj_field_context;

using dynobj_layout_sptr = std::shared_ptr<dynobj_layout>;

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

    static uint64_t
    size(const dynobj_field* f);

    static dynobj_field_context*
    context(const dynobj_field* f);

    const utils::id&
    id() const;

    uint64_t
    field_count() const;

    uint64_t
    glob_offset(uint64_t idx) const;

    uint64_t
    glob_offset(const dynobj_field* f) const;

    uint64_t
    glob_offset_at_index(const dynobj_field* f, uint64_t idx) const;

    static uint64_t
    offset_at_index(const dynobj_field* f, uint64_t idx);

    static const dynobj_field*
    field_by_idx(const utils::dynobj_layout_sptr& layout, uint64_t idx);

    void
    print(std::string& str, bool no_detailds = true) const;

    void
    print_to_std() const;

    template <typename T>
    T*
    context() const
    {
        return (T*)context(m_cur_field);
    }

    const dynobj_field*
    field_by_idx(uint64_t idx) const;

    dynobj_layout_sptr
    layout() const
    {
        return m_layout;
    }

protected:
    void
    print(size_t offset, std::string& str, bool no_detailds) const;

    base_view(uint64_t offset,
              uint8_t* data,
              const dynobj_field* cur_field,
              const utils::dynobj_layout_sptr& layout);

    static uint32_t
    get_type_id(const dynobj_field* f);

    static bool
    is_array(const dynobj_field* f);

    static bool
    is_object(const dynobj_field* f);

    bool
    write_unsafe(const dynobj_field* f, uint64_t offset, uint8_t* src);

    bool
    read_unsafe(const dynobj_field* f, uint64_t offset, uint8_t* dst);

    base_view
    build_for_subobject(uint64_t field_idx) const;

    base_view
    build_for_subobject(uint64_t field_idx, uint64_t idx) const;

    uint64_t m_offset = 0;
    uint8_t* m_data = nullptr;
    const dynobj_field* m_cur_field = nullptr;
    dynobj_layout_sptr m_layout = nullptr;
};

template <typename TYPE_DESCRIPTOR>
class dynobj_view : public base_view
{
public:
    dynobj_view() = default;

    dynobj_view(uint64_t offset,
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
    subobj(uint64_t field_idx) const
    {
        AGEA_check(is_object(), "Should be object!");

        dynobj_view<TYPE_DESCRIPTOR> f(build_for_subobject(field_idx));

        return f;
    }

    dynobj_view
    subobj(uint64_t field_idx, uint64_t idx) const
    {
        AGEA_check(is_object(), "Should be an object!");

        dynobj_view<TYPE_DESCRIPTOR> f(build_for_subobject(field_idx, idx));

        return f;
    }

    template <typename T, typename... VARGS>
    bool
    write_from(uint64_t field_idx, const T& v, VARGS&&... args)
    {
        AGEA_check(is_object(), "Should be object!");

        auto f = field_by_idx(field_idx);

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

        auto f = field_by_idx(0);

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
    write_from(uint64_t field_idx, const T& v)
    {
        AGEA_check(is_object(), "Should be object!");

        auto f = field_by_idx(field_idx);

        if (!f)
        {
            return false;
        }

        return write_impl(f, v);
    }

    template <typename T, typename... VARGS>
    bool
    read(const T& v, VARGS&&... args)
    {
        auto f = field_by_idx(0);

        AGEA_check(!is_array(f), "Should not be array!");

        if (!f || !read_impl(f, v))
        {
            return false;
        }

        return read_from(1, args...);
    }

    template <typename T, typename... VARGS>
    bool
    read_from(uint64_t field_idx, const T& v, VARGS&&... args)
    {
        auto f = field_by_idx(field_idx);

        AGEA_check(!is_array(f), "Should not be array!");

        if (!f || !read_impl(f, v))
        {
            return false;
        }

        return read_from(++field_idx, args...);
    }

    template <typename T, typename... VARGS>
    bool
    read_from(uint64_t field_idx, T& v)
    {
        auto f = field_by_idx(field_idx);

        AGEA_check(!is_array(f), "Should not be array!");

        if (!f)
        {
            return false;
        }

        return read_impl(f, v);
    }

    template <typename... VARGS>
    bool
    write_array(uint64_t field_idx, uint64_t idx, VARGS&&... args)
    {
        AGEA_check(is_object(), "Should be object!");

        auto f = field_by_idx(field_idx);

        if (!f)
        {
            return false;
        }

        AGEA_check(is_array(f), "Should be array!");

        return write_array_impl(f, idx, args...);
    }

    template <typename... VARGS>
    bool
    read_array(uint64_t field_idx, uint64_t idx, VARGS&&... args)
    {
        AGEA_check(is_object(), "Should be object!");

        auto f = field_by_idx(field_idx);

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

        return write_unsafe(dyn_field, glob_offset(dyn_field), (uint8_t*)&v);
    }

    template <typename T, typename... VARGS>
    bool
    write_array_impl(const dynobj_field* dyn_field, uint64_t idx, const T& v, VARGS&&... args)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(dyn_field) != type_id)
        {
            return false;
        }

        auto offset = glob_offset_at_index(dyn_field, idx);

        if (!write_unsafe(dyn_field, offset, (uint8_t*)&v))
        {
            return false;
        }

        return write_array_impl(dyn_field, ++idx, args...);
    }

    template <typename T>
    bool
    write_array_impl(const dynobj_field* dyn_field, uint64_t idx, const T& v)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(dyn_field) != type_id)
        {
            return false;
        }

        auto offset = glob_offset_at_index(dyn_field, idx);

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

        return read_unsafe(dyn_field, glob_offset(dyn_field), (uint8_t*)&v);
    }

    template <typename T, typename... VARGS>
    bool
    read_array_impl(const dynobj_field* dyn_field, uint64_t idx, const T& v, VARGS&&... args)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(dyn_field) != type_id)
        {
            return false;
        }

        auto offset = glob_offset_at_index(dyn_field, idx);

        if (!read_unsafe(dyn_field, offset, (uint8_t*)&v))
        {
            return false;
        }

        return read_array_impl(dyn_field, ++idx, args...);
    }

    template <typename T>
    bool
    read_array_impl(const dynobj_field* dyn_field, uint64_t idx, const T& v)
    {
        auto type_id = TYPE_DESCRIPTOR::decode_as_int(v);

        if (get_type_id(dyn_field) != type_id)
        {
            return false;
        }

        auto offset = glob_offset_at_index(dyn_field, idx);

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
    root() const
    {
        return dynobj_view<TYPE_DESCRIPTOR>(0U, (uint8_t*)data(),
                                            base_view::field_by_idx(m_layout, 0), m_layout);
    }

    bool
    empty() const
    {
        return !(bool)m_layout;
    }

    std::shared_ptr<dynobj_layout>
    get_layout() const
    {
        return m_layout;
    }

private:
    const dynobj_field*
    get_dyn_field(uint64_t idx);

    std::vector<uint8_t> m_obj_data;
    std::shared_ptr<dynobj_layout> m_layout;
};

}  // namespace utils
}  // namespace agea