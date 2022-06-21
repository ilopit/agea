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

}  // namespace utils

}  // namespace agea