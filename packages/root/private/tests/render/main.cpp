#include <gtest/gtest.h>

#include "utils/kryga_log.h"

#include <core/reflection/property.h>
#include <core/reflection/lua_api.h>
#include <core/package_manager.h>
#include <packages/root/package.root.h>

#include <utils/singleton_registry.h>

int
main(int argc, char** argv)
{
    ::kryga::utils::setup_logger();

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}