#include "utils/process.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace kryga
{
namespace ipc
{
namespace
{
const size_t MAX_ARGUMENTS_LENGHT = 300;
}

#if !defined(_WIN32)
bool
run_binary(construct_params, std::uint64_t&)
{
    return false;
}

bool
run_binary_capture(construct_params, std::uint64_t&, std::string&)
{
    return false;
}
}  // namespace ipc
}  // namespace kryga
#else

bool
run_binary(construct_params p, std::uint64_t& result_code)
{
    PROCESS_INFORMATION process_info = {};

    STARTUPINFOA startup_info;
    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof startup_info;
    startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    std::string wd = p.working_dir.str();
    LPSTR wd_ptr = p.working_dir.empty() ? nullptr : wd.data();

    auto path_to_binary = p.path_to_binary.str();
    BOOL result = ::CreateProcessA(path_to_binary.c_str(),
                                   &p.arguments[0],
                                   nullptr,
                                   nullptr,
                                   FALSE,
                                   0,
                                   nullptr,
                                   wd_ptr,
                                   &startup_info,
                                   &process_info);

    if (result)
    {
        const DWORD wait_result = ::WaitForSingleObject(process_info.hProcess, INFINITE);
        if (wait_result != WAIT_OBJECT_0)
        {
            return false;
        }

        DWORD exit_code = 0;
        result = ::GetExitCodeProcess(process_info.hProcess, &exit_code);

        ::CloseHandle(process_info.hThread);
        ::CloseHandle(process_info.hProcess);

        if (result == STILL_ACTIVE)
        {
            return false;
        }

        result_code = exit_code;
        return true;
    }

    return false;
}

bool
run_binary_capture(construct_params p, std::uint64_t& result_code, std::string& captured_output)
{
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;
    if (!::CreatePipe(&read_pipe, &write_pipe, &sa, 0))
    {
        return false;
    }
    ::SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION process_info = {};
    STARTUPINFOA startup_info;
    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof startup_info;
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdOutput = write_pipe;
    startup_info.hStdError = write_pipe;
    startup_info.hStdInput = nullptr;

    std::string wd = p.working_dir.str();
    LPSTR wd_ptr = p.working_dir.empty() ? nullptr : wd.data();

    auto path_to_binary = p.path_to_binary.str();
    BOOL result = ::CreateProcessA(path_to_binary.c_str(),
                                   &p.arguments[0],
                                   nullptr,
                                   nullptr,
                                   TRUE,
                                   0,
                                   nullptr,
                                   wd_ptr,
                                   &startup_info,
                                   &process_info);

    ::CloseHandle(write_pipe);

    if (!result)
    {
        ::CloseHandle(read_pipe);
        return false;
    }

    captured_output.clear();
    char buf[4096];
    DWORD bytes_read = 0;
    while (::ReadFile(read_pipe, buf, sizeof(buf), &bytes_read, nullptr) && bytes_read > 0)
    {
        captured_output.append(buf, bytes_read);
    }
    ::CloseHandle(read_pipe);

    const DWORD wait_result = ::WaitForSingleObject(process_info.hProcess, INFINITE);
    if (wait_result != WAIT_OBJECT_0)
    {
        ::CloseHandle(process_info.hThread);
        ::CloseHandle(process_info.hProcess);
        return false;
    }

    DWORD exit_code = 0;
    result = ::GetExitCodeProcess(process_info.hProcess, &exit_code);

    ::CloseHandle(process_info.hThread);
    ::CloseHandle(process_info.hProcess);

    if (result == STILL_ACTIVE)
    {
        return false;
    }

    result_code = exit_code;
    return true;
}

}  // namespace ipc
}  // namespace kryga
#endif  // _WIN32
