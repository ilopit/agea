#include "utils/id.h"

namespace agea
{
namespace utils
{

id::id()
{
    memset(m_id, 0, id_size_in_bytes() + 1);
}

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
id::make_id(const char* id_cstr)
{
    id result;

    if (strncpy_s(result.m_id, id_size_in_bytes() + 1, id_cstr, id_size_in_bytes()))
    {
        return {};
    }
    return result;
}

id
id::make_id(const std::string& id_str)
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

id
id::make_id(const id& left, const id& right)
{
    id result = left;

    size_t size = strlen(result.m_id);
    size_t right_size = strlen(right.m_id);

    // Prevent buffer overflow: only copy up to remaining capacity, leaving space for null
    // terminator
    size_t copy_len = std::min(right_size, id_size_in_bytes() - size);

    if (copy_len > 0)
    {
        result.m_id[size] = '/';
        memcpy(result.m_id + size + 1, right.m_id, copy_len);
        result.m_id[size + copy_len + 1] = '\0';
    }

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

std::ostream&
operator<<(std::ostream& os, const agea::utils::id& right)
{
    os << right.cstr();

    return os;
}