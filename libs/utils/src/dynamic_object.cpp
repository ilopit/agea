#include "utils/dynamic_object.h"

#include "utils/dynamic_object_builder.h"

namespace agea
{
namespace utils
{

dynamic_object::dynamic_object(const std::shared_ptr<dynamic_object_layout>& l)
    : m_layout(l)
{
}

dynamic_object::dynamic_object(const std::shared_ptr<dynamic_object_layout>& l, size_t size)
    : m_layout(l)
{
    m_data.resize(l->get_size());
}

void
dynamic_object::write_unsafe(uint32_t pos, uint8_t* data)
{
    auto& field = m_layout->get_fields()[pos];

    memcpy(m_data.data() + field.offset, data, field.size);
}

void
dynamic_object::write_unsafe(uint32_t offset, uint32_t field_idx, uint8_t* src)
{
    auto& field = m_layout->get_fields()[field_idx];

    memcpy(data() + field.offset + offset, src, field.size);
}

void
dynamic_object::read_unsafe(uint32_t pos, uint8_t* dst)
{
    auto& field = m_layout->get_fields()[pos];

    memcpy(dst, data() + field.offset, field.size);
}

uint32_t
dynamic_object::size() const
{
    return m_data.size();
}

uint32_t
dynamic_object::expected_size() const
{
    return m_layout->get_size();
}

uint32_t
dynamic_object::get_type_id(uint32_t pos)
{
    return m_layout->get_fields()[pos].type;
}

}  // namespace utils
}  // namespace agea