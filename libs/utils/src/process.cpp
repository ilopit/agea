#include "utils/process.h"

#include <windows.h>

namespace agea
{
namespace ipc
{
namespace
{
const size_t MAX_ARGUMENTS_LENGHT = 300;
}

bool
run_binary(construct_params p, std::uint64_t& result_code)
{
    PROCESS_INFORMATION process_info = {};

    STARTUPINFOA startup_info;
    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof startup_info;
    startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    LPSTR wd_ptr = p.working_dir.empty() ? NULL : p.working_dir.data();

    BOOL result = ::CreateProcessA(p.path_to_binary.c_str(), &p.arguments[0], NULL, NULL, FALSE, 0,
                                   NULL, wd_ptr, &startup_info, &process_info);

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
}  // namespace ipc
}  // namespace agea
