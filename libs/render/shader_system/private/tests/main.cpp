#include <gtest/gtest.h>

#include <global_state/global_state.h>
#include <resource_locator/resource_locator_state.h>
#include <utils/kryga_log.h>

int
main(int argc, char** argv)
{
    kryga::utils::setup_logger();
    kryga::state_mutator__resource_locator::set(kryga::glob::glob_state());

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
