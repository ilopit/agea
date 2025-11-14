#include "base_test.h"

#include <core/global_state.h>

#include <gtest/gtest.h>

using namespace agea;

struct test_global_state : base_test
{
    void
    SetUp()
    {
        base_test::SetUp();
    }
};

TEST_F(test_global_state, all)
{
    agea::glob::state::reset();

    int executing_ctr = 0;

    agea::glob::glob_state().schedule_register([&executing_ctr]() { ++executing_ctr; });

    auto& gs = agea::glob::state::getr();

    core::state_mutator__caches::set(gs);
    ASSERT_TRUE(gs.get_class_cache_map());
    ASSERT_TRUE(gs.get_instance_cache_map());

    ASSERT_EQ(executing_ctr, 0);
    gs.execute_pre_main_actions();
    ASSERT_EQ(executing_ctr, 1);

    agea::glob::state::reset();

    ASSERT_FALSE(gs.get_class_cache_map());
    ASSERT_FALSE(gs.get_instance_cache_map());
    gs.execute_pre_main_actions();
    ASSERT_EQ(executing_ctr, 1);
}