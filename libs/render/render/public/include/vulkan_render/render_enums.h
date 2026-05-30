#pragma once

#include <gpu_types/gpu_shadow_types.h>

#include <cstdint>
#include <string_view>

namespace kryga
{
namespace render
{

enum class pcf_mode : uint32_t
{
    pcf_3x3 = KGPU_PCF_3X3,
    pcf_5x5 = KGPU_PCF_5X5,
    pcf_7x7 = KGPU_PCF_7X7,
    poisson16 = KGPU_PCF_POISSON16,
    poisson32 = KGPU_PCF_POISSON32,
    poisson64 = KGPU_PCF_POISSON64,

    min = KGPU_PCF_MIN,
    max = KGPU_PCF_MAX,
};

struct pcf_mode_info
{
    pcf_mode value;
    const char* name;
    const char* label;
};

inline constexpr pcf_mode_info pcf_mode_entries[] = {
    {pcf_mode::pcf_3x3, "pcf_3x3", "3x3 (9 taps)"},
    {pcf_mode::pcf_5x5, "pcf_5x5", "5x5 (25 taps)"},
    {pcf_mode::pcf_7x7, "pcf_7x7", "7x7 (49 taps)"},
    {pcf_mode::poisson16, "poisson16", "Poisson 16"},
    {pcf_mode::poisson32, "poisson32", "Poisson 32"},
    {pcf_mode::poisson64, "poisson64", "Poisson 64"},
};

inline constexpr int pcf_mode_count = sizeof(pcf_mode_entries) / sizeof(pcf_mode_entries[0]);

inline const char*
to_string(pcf_mode m)
{
    auto idx = static_cast<uint32_t>(m);
    return idx < pcf_mode_count ? pcf_mode_entries[idx].name : "unknown";
}

inline bool
from_string(std::string_view s, pcf_mode& out)
{
    for (auto& e : pcf_mode_entries)
    {
        if (s == e.name)
        {
            out = e.value;
            return true;
        }
    }
    return false;
}

// Swapchain present mode. Decoupled from frames_in_flight: this is an explicit
// vsync choice, not a side-effect of the image count.
//   fifo      — vsync-capped, low power, works with >=2 images (mobile default).
//   mailbox   — low-latency uncapped framerate, REQUIRES >=3 images. Selecting it
//               with frames_in_flight < 3 forces the image count up to 3.
//   immediate — uncapped, no vsync: TEARS. Lowest possible latency when rendering
//               faster than refresh. Not guaranteed by the spec; falls back if the
//               surface doesn't report it.
// Values are contiguous from 0 so they double as indices into the table below.
enum class present_mode : uint32_t
{
    fifo = 0,
    mailbox = 1,
    immediate = 2,
};

struct present_mode_info
{
    present_mode value;
    const char* name;
    const char* label;
};

inline constexpr present_mode_info present_mode_entries[] = {
    {present_mode::fifo, "fifo", "FIFO (vsync)"},
    {present_mode::mailbox, "mailbox", "Mailbox (low-latency)"},
    {present_mode::immediate, "immediate", "Immediate (no vsync, tears)"},
};

inline constexpr int present_mode_count =
    sizeof(present_mode_entries) / sizeof(present_mode_entries[0]);

inline const char*
to_string(present_mode m)
{
    auto idx = static_cast<uint32_t>(m);
    return idx < present_mode_count ? present_mode_entries[idx].name : "unknown";
}

inline bool
from_string(std::string_view s, present_mode& out)
{
    for (auto& e : present_mode_entries)
    {
        if (s == e.name)
        {
            out = e.value;
            return true;
        }
    }
    return false;
}

}  // namespace render
}  // namespace kryga
