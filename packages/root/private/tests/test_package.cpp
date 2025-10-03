#include "base_test.h"

#include <core/reflection/reflection_type.h>
#include <core/id_generator.h>
#include <core/package_manager.h>
#include <core/global_state.h>

#include <packages/root/package.root.h>

using namespace agea;
using namespace core;
using namespace root;

struct test_root_package : public testing::Test
{
    void
    SetUp()
    {
    }

    void
    TearDown()
    {
    }
};

TEST_F(test_root_package, entry_test)
{
    agea::glob::state::reset();

    auto& gs = agea::glob::state::getr();

    gs.schedule_create(
        [](agea::core::state& s)
        {
            state_package_mutator::set(s);
            state_level_mutator::set(s);
            state_reflection_manager_mutator::set(s);
            state_lua_mutator::set(s);

            root::package::instance()
                .register_package_extention<root::package::package_types_loader>();
        });
    gs.run_create();

    gs.schedule_register([](agea::core::state& s)
                         { s.get_pm()->register_static_package(root::package::instance()); });
    gs.run_register();

    gs.schedule_init([](agea::core::state& s) { s.get_pm()->init(); });
    gs.run_init();

    auto rf = gs.get_rm();

    auto go = rf->get_type(AID("game_object"));

    int i = 2;
}