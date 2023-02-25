#pragma once

#include "utils/id.h"
#include "utils/agea_types.h"
#include "utils/check.h"
#include "utils/dynamic_object.h"

#include <vector>
#include <string>
#include <memory>

namespace agea
{
namespace utils
{

struct dynamic_object_field
{
    utils::id id;
    uint32_t type = {};
    uint32_t offset = 0U;
    uint32_t size = 0U;
    uint32_t items_count = 1U;
};

class dynamic_object_layout : public std::enable_shared_from_this<dynamic_object_layout>
{
public:
    template <typename TYPE_DESCRIPTOR>
    friend class dynamic_object_layout_sequence_builder;
    template <typename TYPE_DESCRIPTOR>
    friend class dynamic_object_layout_random_builder;

    dynamic_object_layout() = default;

    const utils::id&
    get_id()
    {
        return m_id;
    }

    uint32_t
    get_size() const
    {
        return m_size;
    }

    const std::vector<dynamic_object_field>&
    get_fields() const
    {
        return m_fields;
    }

    void
    set_id(const utils::id& id)
    {
        m_id = id;
    }

    dynamic_object
    get_empty_obj()
    {
        return dynamic_object(shared_from_this());
    }

private:
    utils::id m_id;
    uint32_t m_size = 0;
    std::vector<dynamic_object_field> m_fields;
};

class basic_dynamic_object_layout_builder
{
public:
    basic_dynamic_object_layout_builder()
        : m_layout(std::make_shared<dynamic_object_layout>())
    {
    }

    std::shared_ptr<dynamic_object_layout>
    get_layout() const
    {
        return m_layout;
    }

private:
    std::shared_ptr<dynamic_object_layout> m_layout;
};

template <typename TYPE_DESCRIPTOR>
class dynamic_object_layout_sequence_builder : public basic_dynamic_object_layout_builder
{
public:
    void
    add_field(const utils::id& id,
              typename TYPE_DESCRIPTOR::id type,
              uint32_t aligment = 4,
              uint32_t items_count = 1)
    {
        dynamic_object_field field;
        field.id = id;
        field.type = uint32_t(type);

        field.size = TYPE_DESCRIPTOR::size(type);

        auto& obj = *basic_dynamic_object_layout_builder::get_layout();

        auto mod = obj.m_size % aligment;

        field.offset = mod ? (obj.m_size + (aligment - mod)) : obj.m_size;

        obj.m_size = field.offset + field.size;

        field.items_count = items_count;

        obj.m_fields.emplace_back(field);
    }
};

template <typename TYPE_DESCRIPTOR>
class dynamic_object_layout_random_builder : public basic_dynamic_object_layout_builder
{
public:
    void
    add_field(const utils::id& id,
              typename TYPE_DESCRIPTOR::id type_id,
              uint32_t offest,
              uint32_t aligment = 4,
              uint32_t items_count = 1)
    {
        auto& obj = *basic_dynamic_object_layout_builder::get_layout();

        dynamic_object_field field;
        field.id = id;
        field.type = type_id;

        auto size = TYPE_DESCRIPTOR::size(type_id);
        field.size = size < aligment ? aligment : size;

        auto mod = obj.m_size % aligment;

        field.offset = offest;

        obj.m_size = field.offset + field.size;

        field.items_count = items_count;

        obj.m_fields.emplace_back(field);
    }

    void
    finalize(uint32_t final_size)
    {
        auto& obj = *basic_dynamic_object_layout_builder::get_layout();
        obj.m_size = final_size;
    }
};

}  // namespace utils

using agea_dynobj_builder = utils::dynamic_object_layout_sequence_builder<utils::agea_type>;
}  // namespace agea