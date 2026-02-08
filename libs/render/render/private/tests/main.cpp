#include <gtest/gtest.h>

#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/kryga_render.h>

#include <global_state/global_state.h>
#include <resource_locator/resource_locator.h>
#include <resource_locator/resource_locator_state.h>

#include <utils/kryga_log.h>

int
main(int argc, char** argv)
{
    ::kryga::utils::setup_logger();

    auto& gs = kryga::glob::glob_state();

    // Initialize resource_locator in global state
    kryga::state_mutator__resource_locator::set(gs);

    kryga::state_mutator__render_device::set(gs);
    kryga::state_mutator__vulkan_render::set(gs);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}