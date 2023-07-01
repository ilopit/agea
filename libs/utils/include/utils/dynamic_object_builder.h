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

struct dynobj_field
{
    utils::id id;

    uint32_t type = 0U;
    uint64_t offset = 0U;
    uint64_t size = 0U;
    uint64_t type_size = 0U;
    uint64_t index = 0U;

    uint64_t items_count = 0U;
    uint64_t items_alighment = 1U;

    dynobj_layout_sptr sub_field_layout;

    bool
    is_array() const
    {
        return items_count;
    }

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

    dynobj_layout_sptr
    unwrap_subojb(const dynobj_layout_sptr& obj);

protected:
    dynobj_layout_sptr m_layout;
};

template <typename TYPE_DESCRIPTOR>
class dynamic_object_layout_sequence_builder : public basic_dynobj_layout_builder
{
public:
    void
    set_id(const utils::id& id)
    {
        m_layout->set_id(id);
    }

    dynamic_object_layout_sequence_builder&
    add_field(const utils::id& id, typename TYPE_DESCRIPTOR::id type, uint64_t aligment = 4)
    {
        dynobj_field field;

        field.type = uint32_t(type);
        field.size = TYPE_DESCRIPTOR::size(type);
        field.type_size = TYPE_DESCRIPTOR::size(type);

        add_field_impl(field, aligment, id);

        return *this;
    }

    dynamic_object_layout_sequence_builder&
    add_field(const utils::id& id, const std::shared_ptr<dynobj_layout>& l, uint64_t aligment = 4)
    {
        dynobj_field field;

        field.type = uint32_t(-1);
        field.size = l->get_object_size();
        field.type_size = l->get_object_size();
        field.sub_field_layout = unwrap_subojb(l);

        add_field_impl(field, aligment, id);

        return *this;
    }

    dynamic_object_layout_sequence_builder&
    add_array(const utils::id& id,
              typename TYPE_DESCRIPTOR::id type,
              uint64_t aligment = 4,
              uint64_t items_count = 1,
              uint64_t item_aligment = 1)
    {
        dynobj_field field;

        field.type = uint32_t(type);
        field.type_size = TYPE_DESCRIPTOR::size(type);
        field.size = math_utils::align_as(field.type_size, item_aligment) * (items_count - 1) +
                     field.type_size;

        field.items_count = items_count;
        field.items_alighment = item_aligment;

        add_field_impl(field, aligment, id);

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

        field.type = uint32_t(-1);
        field.type_size = l->get_object_size();
        field.size = math_utils::align_as(field.type_size, item_aligment) * (items_count - 1) +
                     field.type_size;

        field.items_count = items_count;
        field.items_alighment = item_aligment;
        field.sub_field_layout = unwrap_subojb(l);

        add_field_impl(field, aligment, id);

        return *this;
    }

    std::shared_ptr<dynobj_layout>
    finalize()
    {
        auto root_layout = std::make_shared<dynobj_layout>();
        auto& field = root_layout->m_fields.emplace_back();

        field.type = uint32_t(-1);
        field.size = get_layout()->get_object_size();
        field.type_size = get_layout()->get_object_size();

        root_layout->m_object_size = get_layout()->get_object_size();

        field.sub_field_layout = get_layout();

        return root_layout;
    }

private:
    void
    add_field_impl(dynobj_field& field, uint64_t aligment, const utils::id& id)
    {
        auto& layout = *basic_dynobj_layout_builder::get_layout();

        field.id = id;
        field.offset = math_utils::align_as(layout.m_object_size, aligment);

        layout.m_object_size = field.offset + field.size;

        auto& f = layout.m_fields.emplace_back(field);
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