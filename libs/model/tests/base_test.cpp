#include "base_test.h"

#include <filesystem>

#include <resource_locator/resource_locator.h>

namespace agea
{

void
base_test::SetUp()
{
    std::error_code ec;
    std::filesystem::remove_all(get_current_workspace().fs(), ec);
    std::filesystem::create_directories(get_current_workspace().fs(), ec);

    m_resource_locator = agea::glob::resource_locator::create();
}

void
base_test::TearDown()
{
}

agea::utils::path
base_test::get_current_workspace()
{
    static auto path = std::filesystem::current_path() / "test_workspace";

    return agea::utils::path(path);
}
}  // namespace agea