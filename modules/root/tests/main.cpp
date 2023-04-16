#include <gtest/gtest.h>

#include "utils/agea_log.h"

#include <model/reflection/property.h>
#include <model/caches/empty_objects_cache.h>
#include <model/reflection/lua_api.h>
#include <model/package_manager.h>
#include <root/root_module.h>

#include <utils/singleton_registry.h>

int
main(int argc, char** argv)
{
    ::agea::utils::setup_logger();

    auto registry = std::make_unique<agea::singleton_registry>();
    auto& r = *registry;
    agea::glob::empty_objects_cache::create(r);
    agea::glob::lua_api::create(r);
    agea::glob::package_manager::create(r);
    agea::glob::module_manager::create(r);
    agea::glob::reflection_type_registry::create(r);

    agea::glob::module_manager::getr().register_module<agea::root::root_module>();

    for (auto m : agea::glob::module_manager::getr().modules())
    {
        m->init_reflection();
        m->override_reflection_types();
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}