#include <gtest/gtest.h>

#include <utils/kryga_log.h>

int
main(int argc, char** argv)
{
    kryga::utils::setup_logger();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
