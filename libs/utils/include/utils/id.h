#pragma once

#include <array>
#include <string>

namespace agea
{
namespace utils
{
inline constexpr size_t
id_size_in_bytes()
{
    return 63;
}

class id
{
public:
    id() = default;

    id(const id& other);

    id&
    operator=(const id& other);

    bool
    operator==(const id& other) const;

    bool
    operator!=(const id& other) const;

    static id
    from(const std::string& id_str);

    static id
    from(const char* id_cstr);

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

}  // namespace utils
}  // namespace agea

namespace std
{

template <>
struct hash<::agea::utils::id>
{
    std::size_t
    operator()(const ::agea::utils::id& k) const
    {
        size_t result = 0;
        const size_t prime = 31;

        for (size_t i = 0; i < ::agea::utils::id_size_in_bytes(); ++i)
        {
            if (k.cstr()[i] == '\0')
            {
                break;
            }

            result = k.cstr()[i] + (result * prime);
        }
        return result;
    }
};

}  // namespace std
