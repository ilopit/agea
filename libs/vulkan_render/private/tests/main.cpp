#include <gtest/gtest.h>

#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/kryga_render.h>

#include <resource_locator/resource_locator.h>

#include <utils/kryga_log.h>
#include <utils/singleton_registry.h>

int
main(int argc, char** argv)
{
    ::kryga::utils::setup_logger();

    auto registry = std::make_unique<kryga::singleton_registry>();

    kryga::glob::render_device::create(*registry);
    kryga::glob::resource_locator::create(*registry);
    kryga::glob::vulkan_render::create(*registry);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}