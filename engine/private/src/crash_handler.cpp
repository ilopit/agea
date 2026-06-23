// Minimal in-process crash handler: on an unhandled exception (e.g. an access
// violation), write a full minidump next to the executable, then let the process
// die. Runs ONLY on crash, so it adds zero steady-state overhead and doesn't
// perturb thread timing (unlike attaching a debugger) — important for catching
// timing-sensitive races. Windows-only; a no-op elsewhere.

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>

#include <cstdio>
#include <print>

#pragma comment(lib, "dbghelp.lib")
#endif

namespace kryga
{

#ifdef _WIN32

namespace
{
LONG WINAPI
crash_filter(EXCEPTION_POINTERS* ep)
{
    HANDLE file = CreateFileW(L"kryga_crash.dmp",
                              GENERIC_WRITE,
                              0,
                              nullptr,
                              CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;

        const auto type = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithFullMemory | MiniDumpWithThreadInfo | MiniDumpWithHandleData);

        MiniDumpWriteDump(
            GetCurrentProcess(), GetCurrentProcessId(), file, type, &mei, nullptr, nullptr);
        CloseHandle(file);

        std::println(stderr, "\n*** CRASH: minidump written to kryga_crash.dmp ***");
        fflush(stderr);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
}  // namespace

void
install_crash_handler()
{
    SetUnhandledExceptionFilter(crash_filter);
}

#else

void
install_crash_handler()
{
}

#endif

}  // namespace kryga
