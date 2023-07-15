#pragma once

#include "utils/id.h"
#include "utils/check.h"
#include "utils/dynamic_object.h"
#include "utils/math_utils.h"

#include <vector>
#include <string>
#include <memory>

namespace agea
{
namespace utils
{
class dynobj_layout;

using dynobj_layout_sptr = std::shared_ptr<dynobj_layout>;

struct dynobj_field_context
{
    virtual ~dynobj_field_context()
    {
    }
};

using dynobj_context_uptr = std::unique_ptr<dynobj_field_context>;

struct dynobj_field
{
    utils::id id;

    uint32_t type = 0U;
    uint64_t type_size = 0U;
    const char* type_name = "nan";

    uint64_t offset = 0U;
    uint64_t size = 0U;
    uint64_t alligment = 0U;

    uint64_t items_count = 0U;
    uint64_t items_alighment = 1U;

    dynobj_layout_sptr sub_field_layout;
    dynobj_context_uptr context;

    uint64_t index = 0U;

    bool is_array = false;

    bool
    is_obj() const
    {
        return (bool)sub_field_layout;
    }
};

class dynobj_layout : public std::enable_shared_from_this<dynobj_layout>
{
public:
    template <typename TYPE_DESCRIPTOR>
    friend class dynamic_object_layout_sequence_builder;
    template <typename TYPE_DESCRIPTOR>
    friend class dynamic_object_layout_random_builder;

    dynobj_layout() = default;
    const utils::id&
    get_id()
    {
        return m_id;
    }

    uint64_t
    get_object_size() const
    {
        return m_object_size;
    }

    const std::vector<dynobj_field>&
    get_fields() const
    {
        return m_fields;
    }

    void
    set_id(const utils::id& id)
    {
        m_id = id;
    }

    dynobj
    make_obj()
    {
        return dynobj(shared_from_this());
    }

    template <typename T>
    dynobj_view<T>
    make_view(void* data = nullptr)
    {
        return dynobj_view<T>(0U, (uint8_t*)data, m_fields.data(), shared_from_this());
    }

    dynobj_field&
    back()
    {
        return m_fields.back();
    }

private:
    utils::id m_id;
    uint64_t m_object_size = 0;
    std::vector<dynobj_field> m_fields;
};

class basic_dynobj_layout_builder
{
public:
    basic_dynobj_layout_builder()
        : m_layout(std::make_shared<dynobj_layout>())
    {
    }

    dynobj_layout_sptr
    get_layout() const
    {
        return m_layout;
    }

    static dynobj_layout_sptr
    unwrap_subojb(const dynobj_layout_sptr& obj);

    dynobj_field&
    last_field()
    {
        return m_layout->back();
    }

    bool
    empty()
    {
        return m_layout->get_fields().empty();
    }

protected:
    utils::id m_id;
    dynobj_layout_sptr m_layout;
};

template <typename TYPE_DESCRIPTOR>
class dynamic_object_layout_sequence_builder : public basic_dynobj_layout_builder
{
public:
    using TYPE_ID = typename TYPE_DESCRIPTOR::id;

    static void
    finalize_field(typename TYPE_ID type, dynobj_field& f)
    {
        f.type = uint32_t(type);

        if (f.is_obj())
        {
            f.type_size = f.sub_field_layout->get_object_size();
            f.sub_field_layout = unwrap_subojb(f.sub_field_layout);
        }
        else
        {
            f.type_size = TYPE_DESCRIPTOR::size(type);
            f.type_name = TYPE_DESCRIPTOR::name(type);
        }

        if (!f.is_array)
        {
            f.size = f.type_size;
        }
        else
        {
            f.size = math_utils::align_as(f.type_size, f.items_alighment) * (f.items_count - 1) +
                     f.type_size;
        }
    }

    dynamic_object_layout_sequence_builder&
    set_id(const utils::id& id)
    {
        get_layout()->set_id(id);
        return *this;
    }

    dynamic_object_layout_sequence_builder&
    add_empty()
    {
        dynobj_field field;
        field.type_name = "nan";
        field.id = AID("#empty");

        add_field_impl(field, true);

        return *this;
    }

    dynamic_object_layout_sequence_builder&
    add_field(dynobj_field field)
    {
        add_field_impl(field);

        return *this;
    }

    dynamic_object_layout_sequence_builder&
    add_field(const utils::id& id, typename TYPE_DESCRIPTOR::id type, uint64_t aligment = 4)
    {
        dynobj_field field;
        field.alligment = aligment;
        field.id = id;

        finalize_field(type, field);

        add_field_impl(field);

        return *this;
    }

    template <typename T>
    dynamic_object_layout_sequence_builder&
    add_field(const utils::id& id,
              TYPE_ID type,
              uint64_t aligment = 4,
              std::unique_ptr<T> context = nullptr)
    {
        dynobj_field field;

        field.alligment = aligment;
        field.id = id;
        field.context = std::move(context);

        finalize_field(type, field);

        add_field_impl(field, aligment, id);

        return *this;
    }

    dynamic_object_layout_sequence_builder&
    add_field(const utils::id& id, const std::shared_ptr<dynobj_layout>& l, uint64_t aligment = 4)
    {
        dynobj_field field;

        field.alligment = aligment;
        field.id = id;
        field.sub_field_layout = l;

        finalize_field(TYPE_ID::nan, field);

        add_field_impl(field);

        return *this;
    }

    template <typename T>
    dynamic_object_layout_sequence_builder&
    add_field(const utils::id& id,
              const std::shared_ptr<dynobj_layout>& l,
              uint64_t aligment = 4,
              std::unique_ptr<T> context = nullptr)
    {
        dynobj_field field;

        field.alligment = aligment;
        field.id = id;
        field.sub_field_layout = l;
        field.context = std::move(context);

        finalize_field(TYPE_ID::nan, field);

        add_field_impl(field);

        return *this;
    }

    dynamic_object_layout_sequence_builder&
    add_array(const utils::id& id,
              TYPE_ID type,
              uint64_t aligment = 4,
              uint64_t items_count = 1,
              uint64_t item_aligment = 1)
    {
        dynobj_field field;
        field.is_array = true;
        field.alligment = aligment;
        field.id = id;
        field.items_count = items_count;
        field.items_alighment = item_aligment;

        finalize_field(type, field);

        add_field_impl(field);

        return *this;
    }

    dynamic_object_layout_sequence_builder&
    add_array(const utils::id& id,
              const std::shared_ptr<dynobj_layout>& l,
              uint64_t aligment = 4,
              uint64_t items_count = 1,
              uint64_t item_aligment = 1)
    {
        dynobj_field field;

        field.is_array = true;
        field.alligment = aligment;
        field.id = id;
        field.items_count = items_count;
        field.items_alighment = item_aligment;
        field.sub_field_layout = l;

        finalize_field(TYPE_ID::nan, field);

        add_field_impl(field);

        return *this;
    }

    std::shared_ptr<dynobj_layout>
    finalize()
    {
        auto root_layout = std::make_shared<dynobj_layout>();
        auto& field = root_layout->m_fields.emplace_back();

        field.type = uint32_t(TYPE_ID::nan);
        field.size = get_layout()->get_object_size();
        field.type_size = get_layout()->get_object_size();

        field.id = m_id.valid() ? m_id : get_layout()->get_id();

        root_layout->m_object_size = get_layout()->get_object_size();

        field.sub_field_layout = get_layout();

        return root_layout;
    }

private:
    void
    add_field_impl(dynobj_field& field, bool empty = false)
    {
        auto& layout = *basic_dynobj_layout_builder::get_layout();

        if (!empty)
        {
            field.offset = math_utils::align_as(layout.m_object_size, field.alligment);
            layout.m_object_size = field.offset + field.size;
        }

        auto& f = layout.m_fields.emplace_back(std::move(field));
        f.index = (layout.m_fields.size() - 1);
    }
};

template <typename TYPE_DESCRIPTOR>
class dynamic_object_layout_random_builder : public basic_dynobj_layout_builder
{
public:
    void
    add_field(const utils::id& id,
              typename TYPE_DESCRIPTOR::id type_id,
              uint64_t offest,
              uint64_t aligment = 4,
              uint64_t items_count = 1)
    {
        auto& obj = *basic_dynobj_layout_builder::get_layout();

        dynobj_field field;
        field.id = id;
        field.type = type_id;

        auto size = TYPE_DESCRIPTOR::size(type_id);
        field.size = size < aligment ? aligment : size;

        auto mod = obj.m_object_size % aligment;

        field.offset = offest;

        obj.m_object_size = field.offset + field.size;

        field.items_count = items_count;

        obj.m_fields.emplace_back(field);
    }

    void
    finalize(uint64_t final_size)
    {
        auto& obj = *basic_dynobj_layout_builder::get_layout();
        obj.m_object_size = final_size;
    }
};

}  // namespace utils

}  // namespace agea