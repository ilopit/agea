#pragma once

#include <cstdint>

namespace agea
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

    static uint64_t
    align_as_pow2(uint64_t value, uint64_t align)
    {
        return (value + align - 1) & ~(align - 1);
    }
};
}  // namespace utils
}  // namespace agea