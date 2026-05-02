#include <gtest/gtest.h>

#include <project_paths/project_paths.h>
#include <utils/kryga_log.h>

int
main(int argc, char** argv)
{
    kryga::utils::setup_logger(spdlog::level::level_enum::info);

    // Hand argv[0] to the resolver before any TEST() body runs.
    kryga::paths::set_exe_path(argc > 0 ? argv[0] : nullptr);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
