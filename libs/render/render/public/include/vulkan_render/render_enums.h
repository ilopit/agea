#pragma once

#include <gpu_types/gpu_shadow_types.h>
#include <utils/kryga_enum.h>

#include <cstdint>
#include <string_view>

namespace kryga
{
namespace render
{

// Row: (enumerator, value, yaml-name, ui-label). The yaml-name is the stable
// serialization key written to .acfg — keep it short and do NOT rename it
// casually (it would orphan existing config files).
// clang-format off
#define KRG_PCF_MODE_LIST(X)                                       \
    X(pcf_3x3,    KGPU_PCF_3X3,       "3x3",       "3x3 (9 taps)")  \
    X(pcf_5x5,    KGPU_PCF_5X5,       "5x5",       "5x5 (25 taps)") \
    X(pcf_7x7,    KGPU_PCF_7X7,       "7x7",       "7x7 (49 taps)") \
    X(poisson16,  KGPU_PCF_POISSON16, "poisson16", "Poisson 16")   \
    X(poisson32,  KGPU_PCF_POISSON32, "poisson32", "Poisson 32")   \
    X(poisson64,  KGPU_PCF_POISSON64, "poisson64", "Poisson 64")   \
    X(pcss,       KGPU_PCF_PCSS,      "pcss",      "PCSS (soft)")
// clang-format on

KRG_declare_enum(pcf_mode, uint32_t, KRG_PCF_MODE_LIST)

    // Inclusive numeric range, for clamping a deserialized value. Kept out of the
    // list so they don't show up as selectable entries / UI combo items.
    inline constexpr pcf_mode pcf_mode_min = static_cast<pcf_mode>(KGPU_PCF_MIN);
inline constexpr pcf_mode pcf_mode_max = static_cast<pcf_mode>(KGPU_PCF_MAX);

// Swapchain present mode. Decoupled from frames_in_flight: this is an explicit
// vsync choice, not a side-effect of the image count.
//   fifo      — vsync-capped, low power, works with >=2 images (mobile default).
//   mailbox   — low-latency uncapped framerate, REQUIRES >=3 images. Selecting it
//               with frames_in_flight < 3 forces the image count up to 3.
//   immediate — uncapped, no vsync: TEARS. Lowest possible latency when rendering
//               faster than refresh. Not guaranteed by the spec; falls back if the
//               surface doesn't report it.
// Values are contiguous from 0 so they double as indices into the table.
// clang-format off
#define KRG_PRESENT_MODE_LIST(X)                                          \
    X(fifo,      0, "fifo",      "FIFO (vsync)")                           \
    X(mailbox,   1, "mailbox",   "Mailbox (low-latency)")                  \
    X(immediate, 2, "immediate", "Immediate (no vsync, tears)")
// clang-format on

KRG_declare_enum(present_mode, uint32_t, KRG_PRESENT_MODE_LIST)

}  // namespace render
}  // namespace kryga
