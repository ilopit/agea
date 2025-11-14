
#include "core/object_constructor.h"

#include <core/package_manager.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <core/global_state.h>
#include <core/reflection/reflection_type.h>
#include <testing/testing.h>

#include "packages/root/package.root.h"

#include <utils/agea_log.h>
#include <utils/file_utils.h>

#include <gtest/gtest.h>

using namespace agea;

struct test_load_level : public base_test
{
    void
    SetUp()
    {
        glob::glob_state_reset();
        base_test::SetUp();
    }

    void
    TearDown()
    {
        glob::glob_state_reset();
        base_test::TearDown();
    }
};

TEST_F(test_load_level, basic_load)
{
    auto& gs = glob::glob_state();

    ///
    gs.schedule_action(core::state::state_stage::create,
                       [](core::state& s)
                       {
                           // state
                           core::state_mutator__caches::set(s);
                           core::state_mutator__level_manager::set(s);
                           core::state_mutator__package_manager::set(s);
                           core::state_mutator__reflection_manager::set(s);
                           core::state_mutator__id_generator::set(s);
                           core::state_mutator__lua_api::set(s);
                       });
    gs.run_create();

    root::package::init_instance();
    auto& pkg = root::package::instance();
    pkg.register_package_extention<root::package::package_types_builder>();
    pkg.register_package_extention<root::package::package_types_default_objects_builder>();

    auto& lm = gs.getr_lm();

    auto l = lm.load_level(AID(""));

    int i = 2;
}