#include <gtest/gtest.h>

#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/vulkan_render.h>

#include <resource_locator/resource_locator.h>

#include <utils/agea_log.h>
#include <utils/singleton_registry.h>

int
main(int argc, char** argv)
{
    ::agea::utils::setup_logger();

    auto registry = std::make_unique<agea::singleton_registry>();

    agea::glob::render_device::create(*registry);
    agea::glob::resource_locator::create(*registry);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}