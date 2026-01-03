#include "base_test.h"

#include <filesystem>

#include <resource_locator/resource_locator.h>

namespace kryga
{

void
base_test::SetUp()
{
    std::error_code ec;
    std::filesystem::remove_all(get_current_workspace().fs(), ec);
    std::filesystem::create_directories(get_current_workspace().fs(), ec);

    kryga::glob::resource_locator::create(m_regestry);
}

void
base_test::TearDown()
{
}

kryga::utils::path
base_test::get_current_workspace()
{
    static auto path = std::filesystem::current_path() / "test_workspace";

    return kryga::utils::path(path);
}
}  // namespace kryga