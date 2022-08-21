#pragma once

#include "utils/id.h"

#include "utils/check.h"

#include <vector>
#include <string>
#include <memory>

namespace agea
{
namespace utils
{
struct dymamic_object_wrapper
{
    template <typename T>
    void
    init(const id& i, const T& obj = T())
    {
        m_id = i;
        m_data.resize(sizeof(T));

        memcpy(m_data.data(), &obj, sizeof(T));
    }

    template <typename T>
    T&
    as()
    {
        AGEA_check(m_data.size() == sizeof(T), "Should be same!");
        return *((T*)m_data.data());
    }

    template <typename T>
    const T&
    as() const
    {
        AGEA_check(m_data.size() == sizeof(T), "Should be same!");
        return *((T*)m_data.data());
    }

    id m_id;
    std::vector<uint8_t> m_data;
};

struct dynamic_object_field
{
    utils::id id;
    uint32_t type_id;
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t items_count = 0;
};

class dynamic_object_layout
{
public:
    void
    add_field(
        utils::id id, uint32_t type, uint32_t size, uint32_t aligment = 4, uint32_t items_count = 0)
    {
        dynamic_object_field field;
        field.id = id;
        field.type_id = type;

        field.size = size < aligment ? aligment : size;

        auto mod = m_size % aligment;

        field.offset = mod ? (m_size + (aligment - mod)) : m_size;

        m_size = field.offset + field.size;

        field.items_count = items_count;

        m_fields.emplace_back(field);
    }

    void
    finalize(uint32_t alligment)
    {
        auto mod = m_size % alligment;
        m_size = mod ? (m_size + (alligment - mod)) : m_size;
    }

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
    get_fields()
    {
        return m_fields;
    }

    void
    set_id(const utils::id& id)
    {
        m_id = id;
    }

private:
    utils::id m_id;
    uint32_t m_size = 0;
    std::vector<dynamic_object_field> m_fields;
};

struct dynamic_object
{
    dynamic_object() = default;

    dynamic_object(const std::shared_ptr<dynamic_object_layout>& l)
        : m_layout(l)
    {
        m_data.resize(l->get_size());
    }

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

private:
    std::vector<uint8_t> m_data;
    std::shared_ptr<dynamic_object_layout> m_layout;
};

}  // namespace utils
}  // namespace agea