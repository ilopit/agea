#pragma once

#include <cstdint>

#include <array>

namespace agea
{
namespace utils
{
uint64_t
get_current_time_mks();

template <size_t range = 16>
struct counter
{
    uint64_t value = 0;
    double avg = 0;
    uint64_t avg_sum = 0;

    void
    update(uint64_t v)
    {
        value = v;
        auto prev_v = values[idx];
        values[idx] = v;
        idx = (++idx % range);
        avg_sum += v;
        avg_sum -= prev_v;
        avg = avg_sum / double(range);
    }

private:
    std::array<uint64_t, range> values = {};
    size_t idx = 0;
};
}  // namespace utils
}  // namespace agea