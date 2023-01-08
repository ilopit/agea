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
    agea_type type_id;
    uint32_t offset = 0U;
    uint32_t size = 0U;
    uint32_t items_count = 1U;
};

class dynamic_object_layout;

class basic_dynamic_object_layout_builder
{
public:
    basic_dynamic_object_layout_builder();

    std::shared_ptr<dynamic_object_layout>
    get_obj() const
    {
        return m_layout;
    }

private:
    std::shared_ptr<dynamic_object_layout> m_layout;
};

class dynamic_object_layout_sequence_builder : public basic_dynamic_object_layout_builder
{
public:
    void
    add_field(const utils::id& id, agea_type type, uint32_t aligment = 4, uint32_t items_count = 1);
};

class dynamic_object_layout_random_builder : public basic_dynamic_object_layout_builder
{
public:
    void
    add_field(const utils::id& id,
              agea_type type,
              uint32_t offest,
              uint32_t aligment = 4,
              uint32_t items_count = 1);

    void
    finalize(uint32_t final_size);
};

class dynamic_object_layout
{
public:
    friend class dynamic_object_layout_sequence_builder;
    friend class dynamic_object_layout_random_builder;

    dynamic_object_layout();
    dynamic_object_layout(bool partial_description);

    const utils::id&
    get_id();

    uint32_t
    get_size() const;

    const std::vector<dynamic_object_field>&
    get_fields();

    void
    set_id(const utils::id& id);

private:
    bool m_partial_description = false;
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