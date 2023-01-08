#include "utils/dynamic_object.h"

namespace agea
{
namespace utils
{

dynamic_object_layout::dynamic_object_layout()
{
}

dynamic_object_layout::dynamic_object_layout(bool partial_description)
    : m_partial_description(partial_description)
{
}

const id&
dynamic_object_layout::get_id()
{
    return m_id;
}

uint32_t
dynamic_object_layout::get_size() const
{
    return m_size;
}

const std::vector<dynamic_object_field>&
dynamic_object_layout::get_fields()
{
    return m_fields;
}

void
dynamic_object_layout::set_id(const utils::id& id)
{
    m_id = id;
}

void
dynamic_object_layout_sequence_builder::add_field(const utils::id& id,
                                                  agea_type type,
                                                  uint32_t aligment /*= 4*/,
                                                  uint32_t items_count /*= 1*/)
{
    dynamic_object_field field;
    field.id = id;
    field.type_id = type;

    auto size = get_agea_type_size(type);
    field.size = size < aligment ? aligment : size;

    auto& obj = *get_obj();

    auto mod = obj.m_size % aligment;

    field.offset = mod ? (obj.m_size + (aligment - mod)) : obj.m_size;

    obj.m_size = field.offset + field.size;

    field.items_count = items_count;

    obj.m_fields.emplace_back(field);
}

void
dynamic_object_layout_random_builder::add_field(const utils::id& id,
                                                agea_type type,
                                                uint32_t offest,
                                                uint32_t aligment /*= 4*/,
                                                uint32_t items_count /*= 1*/)
{
    auto& obj = *get_obj();

    dynamic_object_field field;
    field.id = id;
    field.type_id = type;

    auto size = get_agea_type_size(type);
    field.size = size < aligment ? aligment : size;

    auto mod = obj.m_size % aligment;

    field.offset = offest;

    obj.m_size = field.offset + field.size;

    field.items_count = items_count;

    obj.m_fields.emplace_back(field);
}

void
dynamic_object_layout_random_builder::finalize(uint32_t final_size)
{
    auto& obj = *get_obj();
    obj.m_size = final_size;
}

basic_dynamic_object_layout_builder::basic_dynamic_object_layout_builder()
    : m_layout(std::make_shared<dynamic_object_layout>())
{
}

}  // namespace utils
}  // namespace agea