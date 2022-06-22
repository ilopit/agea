#pragma once

#include <array>
#include <string>

namespace agea
{
namespace core
{
inline constexpr size_t
id_size_in_bytes()
{
    return 32;
}

class id
{
public:
    id() = default;

    static id
    from(const std::string& id_str)
    {
        id result;
        if (id_str.empty() || id_str.size() > id_size_in_bytes())
        {
            return result;
        }

        memcpy(result.m_id, id_str.data(), id_str.size());
        result.m_id[id_str.size()] = '\0';

        return result;
    }

    static id
    from(const char* id_cstr)
    {
        id result;

        if (!strncpy_s(result.m_id, id_size_in_bytes(), id_cstr, id_size_in_bytes()))
        {
            return {};
        }
        return result;
    }

    std::string
    str() const
    {
        return m_id;
    }

    const char*
    cstr() const
    {
        return m_id;
    }

    bool
    valid()
    {
        return m_id[0] != '\0';
    }

private:
    char m_id[id_size_in_bytes() + 1] = {0};
};
}  // namespace core

}  // namespace agea