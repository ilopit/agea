#include <gtest/gtest.h>

#include "utils/agea_log.h"

#include <core/reflection/property.h>
#include <core/reflection/lua_api.h>
#include <core/package_manager.h>

#include <utils/singleton_registry.h>

int
main(int argc, char** argv)
{
    ::agea::utils::setup_logger();

    auto registry = std::make_unique<agea::singleton_registry>();
    auto& r = *registry;
    agea::glob::lua_api::create(r);
    agea::glob::package_manager::create(r);
    //   agea::glob::reflection_type_registry::create(r);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}