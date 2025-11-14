#include <gtest/gtest.h>

#include "utils/agea_log.h"

#include <core/reflection/property.h>
#include <core/reflection/lua_api.h>
#include <core/package_manager.h>

#include <utils/singleton_registry.h>

int
main(int argc, char** argv)
{
    ::agea::utils::setup_logger(spdlog::level::level_enum::trace);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
\