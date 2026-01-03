#include <gtest/gtest.h>

#include "utils/kryga_log.h"

#include <core/reflection/property.h>
#include <core/reflection/lua_api.h>
#include <core/package_manager.h>

#include <utils/singleton_registry.h>

int
main(int argc, char** argv)
{
    ::kryga::utils::setup_logger();

    auto registry = std::make_unique<kryga::singleton_registry>();
    auto& r = *registry;
    kryga::glob::lua_api::create(r);
    kryga::glob::package_manager::create(r);
    //     kryga::glob::module_manager::create(r);
    //     kryga::glob::reflection_type_registry::create(r);
    //
    //     kryga::glob::module_manager::getr().register_module<kryga::core::model_module>();
    //
    //     for (auto& [id, m] : kryga::glob::module_manager::getr().modules())
    //     {
    //         m->init_types();
    //         m->init_reflection();
    //     }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}