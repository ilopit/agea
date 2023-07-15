#include <gtest/gtest.h>

#include <utils/agea_log.h>
#include <utils/singleton_registry.h>
#include <core/reflection/reflection_type.h>
#include <core/reflection/lua_api.h>
#include <core/package_manager.h>
#include <root/root_module.h>
#include <resource_locator/resource_locator.h>

using namespace agea;
int
main(int argc, char** argv)
{
    ::agea::utils::setup_logger();

    auto registry = std::make_unique<agea::singleton_registry>();

    glob::module_manager::create(*registry);
    glob::lua_api::create(*registry);
    glob::reflection_type_registry::create(*registry);
    glob::package_manager::create(*registry);

    glob::module_manager::getr().register_module<root::root_module>();

    for (auto m : glob::module_manager::getr().modules())
    {
        m->init_reflection();
        m->override_reflection_types();
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}