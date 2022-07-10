#include "utils/path.h"

namespace agea
{
namespace utils
{

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