#pragma once

#include <cstdint>

namespace kryga
{
namespace utils
{
struct math_utils
{
    static uint64_t
    align_as(uint64_t value, uint64_t k)
    {
        uint64_t mod = value % k;
        return mod ? (value + (k - mod)) : value;
    }
};
}  // namespace utils
}  // namespace kryga