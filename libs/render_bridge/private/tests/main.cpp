#include <gtest/gtest.h>

#include <utils/kryga_log.h>
#include <utils/singleton_registry.h>
#include <core/reflection/reflection_type.h>
#include <core/reflection/lua_api.h>
#include <core/package_manager.h>
#include <root/root_module.h>

using namespace kryga;
int
main(int argc, char** argv)
{
    ::kryga::utils::setup_logger();

    auto registry = std::make_unique<kryga::singleton_registry>();

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