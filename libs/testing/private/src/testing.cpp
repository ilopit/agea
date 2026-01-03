#include "testing/testing.h"

namespace kryga
{

void
testing_base::Do_SetUp()
{
    std::error_code ec;
    std::filesystem::remove_all(get_current_workspace().fs(), ec);
    std::filesystem::create_directories(get_current_workspace().fs(), ec);
}

void
testing_base::Do_TearDown()
{
}

kryga::utils::path
testing_base::get_current_workspace()
{
    static auto path = std::filesystem::current_path() / "test_workspace";

    return kryga::utils::path(path);
}
}  // namespace kryga