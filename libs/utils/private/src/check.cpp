#include "utils/check.h"

#include <spdlog/spdlog.h>
#include <cstdio>
#include <cstdlib>

#if KRG_PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>  // _exit
#endif

namespace kryga::utils
{

bool
is_debugger_attached()
{
#if KRG_PLATFORM_WIN32
    return ::IsDebuggerPresent() != 0;
#else
    return false;
#endif
}

[[noreturn]] void
assert_fail(const char* expr, const char* file, int line)
{
    auto logger = spdlog::default_logger();
    if (logger)
    {
        logger->critical("ASSERT FAILED: {} ({}:{})", expr, file, line);
        logger->flush();
    }
    else
    {
        fprintf(stderr, "ASSERT FAILED: %s (%s:%d)\n", expr, file, line);
        fflush(stderr);
    }

    if (is_debugger_attached())
    {
        KRG_DEBUGBREAK();
    }

#if KRG_PLATFORM_WIN32
    TerminateProcess(GetCurrentProcess(), 3);
#else
    _exit(3);
#endif
}

}  // namespace kryga::utils
