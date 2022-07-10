#include "utils/id.h"

namespace agea
{
namespace utils
{
bool
id::operator==(const id& other) const
{
    return strncmp(other.m_id, m_id, id_size_in_bytes()) == 0;
}

bool
id::operator!=(const id& other) const
{
    return !(*this == other);
}

id
id::from(const char* id_cstr)
{
    id result;

    if (strncpy_s(result.m_id, id_size_in_bytes() + 1, id_cstr, id_size_in_bytes()))
    {
        return {};
    }
    return result;
}

id
id::from(const std::string& id_str)
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

id::id(const id& other)
{
    memcpy(m_id, other.m_id, id_size_in_bytes() + 1);
}

id&
id::operator=(const id& other)
{
    if (this != &other)
    {
        memcpy(m_id, other.m_id, id_size_in_bytes() + 1);
    }

    return *this;
}

}  // namespace utils
}  // namespace agea