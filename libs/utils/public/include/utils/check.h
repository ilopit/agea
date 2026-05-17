#pragma once

#include "kryga_port/platform.h"

#if KRG_PLATFORM_WIN32
#define KRG_DEBUGBREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#define KRG_DEBUGBREAK() __builtin_trap()
#else
#define KRG_DEBUGBREAK() ((void)0)
#endif

namespace kryga::utils
{
[[noreturn]] void
assert_fail(const char* expr, const char* file, int line);

bool
is_debugger_attached();
}  // namespace kryga::utils

#define KRG_check(condition, msg)                                                  \
    do                                                                             \
    {                                                                              \
        if (!(condition))                                                          \
        {                                                                          \
            ::kryga::utils::assert_fail(#condition " — " msg, __FILE__, __LINE__); \
        }                                                                          \
    } while (0)

#define KRG_never(msg)                                        \
    do                                                        \
    {                                                         \
        ::kryga::utils::assert_fail(msg, __FILE__, __LINE__); \
    } while (0)

#define KRG_not_implemented KRG_never("Not Implemented!")
#define KRG_deprecate KRG_never("Deprecated!")
