#include "testing/testing.h"

namespace agea
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

agea::utils::path
testing_base::get_current_workspace()
{
    static auto path = std::filesystem::current_path() / "test_workspace";

    return agea::utils::path(path);
}
}  // namespace agea