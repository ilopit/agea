#include "utils/dynamic_object.h"

#include "utils/dynamic_object_builder.h"

namespace agea
{
namespace utils
{

dynamic_object::dynamic_object(const std::shared_ptr<dynamic_object_layout>& l)
    : m_external_ref(nullptr)
    , m_data(nullptr)
    , m_layout(l)
{
}

dynamic_object::dynamic_object(const std::shared_ptr<dynamic_object_layout>& l, size_t size)
    : m_allocated(size)
    , m_data(&m_allocated)
    , m_layout(l)
{
    int i = 2;
}

dynamic_object::dynamic_object(const std::shared_ptr<dynamic_object_layout>& l,
                               std::vector<uint8_t>* external)
    : m_external_ref(external)
    , m_data(m_external_ref)
    , m_layout(l)
{
}

dynamic_object::dynamic_object(const dynamic_object& other)
    : m_allocated(other.m_allocated)
    , m_external_ref(other.m_external_ref)
    , m_data(m_external_ref ? m_external_ref : &m_allocated)
    , m_layout(other.m_layout)
{
}
dynamic_object&
dynamic_object::operator=(const dynamic_object& other)
{
    if (this != &other)
    {
        m_allocated = other.m_allocated;
        m_external_ref = other.m_external_ref;
        m_data = m_external_ref ? m_external_ref : &m_allocated;
        m_layout = other.m_layout;
    }

    return *this;
}

void
dynamic_object::write_unsafe(uint32_t pos, uint8_t* data)
{
    auto& field = m_layout->get_fields()[pos];

    memcpy(m_data->data() + field.offset, data, field.size);
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
    return m_data->size();
}

uint32_t
dynamic_object::expected_size() const
{
    return m_layout->get_object_size();
}

uint32_t
dynamic_object::get_type_id(uint32_t pos)
{
    return m_layout->get_fields()[pos].type;
}

}  // namespace utils
}  // namespace agea