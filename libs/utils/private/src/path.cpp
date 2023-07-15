#include "utils/path.h"

namespace agea
{
namespace utils
{

path
path::relative(const path& p) const
{
    return path(std::filesystem::relative(m_value, p.fs()));
}

bool
path::exists() const
{
    return std::filesystem::exists(m_value);
}

bool
path::empty() const
{
    return m_value.empty();
}

}  // namespace utils

}  // namespace agea