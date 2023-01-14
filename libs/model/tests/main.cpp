#include <gtest/gtest.h>

#include "utils/agea_log.h"

#include <model/reflection/property.h>
#include <model/caches/empty_objects_cache.h>
#include <model/reflection/lua_api.h>

#include <utils/singleton_registry.h>

int
main(int argc, char** argv)
{
    ::agea::utils::setup_logger();

    auto registry = std::make_unique<agea::singleton_registry>();
    auto& r = *registry;
    agea::glob::empty_objects_cache::create(r);
    agea::glob::lua_api::create(r);

    ::agea::reflection::entry::set_up();

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}