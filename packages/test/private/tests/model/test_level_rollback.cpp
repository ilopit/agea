#include <core/level.h>
#include <core/model_system.h>
#include <core/package_manager.h>
#include <core/core_state.h>
#include <global_state/global_state.h>
#include <vfs/vfs_state.h>
#include <vfs/vfs.h>
#include <vfs/physical_backend.h>
#include <testing/testing.h>

#include "packages/root/package.root.h"
#include "packages/root/package.root.types_builder.ar.h"
#include "packages/base/package.base.h"
#include "packages/base/package.base.types_builder.ar.h"
#include "packages/test/package.test.h"

#include <packages/root/model/game_object.h>

#include <gtest/gtest.h>

using namespace kryga;

// Unit tests for level::snapshot()/rollback() (play-mode option B). These cover
// the model-state outcomes of a play session — survivor reset, spawned cleanup,
// destroyed-survivor revive — independent of the render bridge (rollback only
// enqueues render work). See docs/plans/play-mode-state-snapshot.md.
struct test_level_rollback : base_test
{
    void
    SetUp()
    {
        glob::glob_state_reset();

        auto& gs = glob::glob_state();
        state_mutator__vfs::set(gs);
        {
            auto root = std::filesystem::current_path().parent_path();
            auto& vfs = gs.getr_vfs();
            vfs.mount("data", std::make_unique<vfs::physical_backend>(root), 0);
            vfs.mount("cache", std::make_unique<vfs::physical_backend>(root / "cache"), 0);
            vfs.mount("tmp", std::make_unique<vfs::physical_backend>(root / "tmp"), 0);
            vfs.mount(
                "generated",
                std::make_unique<vfs::physical_backend>(root.parent_path() / "kryga_generated"),
                0);
        }
        core::state_mutator__lua_api::set(gs);
        core::state_mutator__model::set(gs);
        state_mutator__queues::set(gs);
        auto& pm = gs.getr_model().packages;

        gs.run_create();
        {
            pm.register_static_package_loader<root::package>();
            auto& pkg = pm.load_static_package<root::package>();
            pkg.register_package_extension<root::package::package_types_builder>();
            pkg.complete_load();
        }
        {
            pm.register_static_package_loader<base::package>();
            auto& pkg = pm.load_static_package<base::package>();
            pkg.register_package_extension<base::package::package_types_builder>();
            pkg.complete_load();
        }
        {
            pm.register_static_package_loader<test::package>();
            auto& pkg = pm.load_static_package<test::package>();
            pkg.init();
            pkg.finalize_reflection();
        }
    }

    void
    TearDown()
    {
        test::package::instance().unload();
        base::package::instance().unload();
        root::package::instance().unload();
        glob::glob_state_reset();

        base_test::TearDown();
    }

    static root::game_object*
    spawn(core::level& lvl, const char* id, root::vec3 pos)
    {
        root::game_object::construct_params p;
        p.pos = pos;
        return lvl.spawn_object<root::game_object>(AID(id), p);
    }
};

// A survivor mutated during play is reset to its pre-play value on rollback.
TEST_F(test_level_rollback, survivor_value_restored)
{
    core::level lvl(AID("rb_lvl_1"));
    auto* go = spawn(lvl, "rb_hero", root::vec3(1.f, 2.f, 3.f));
    ASSERT_TRUE(go);

    lvl.snapshot();
    go->set_position(root::vec3(9.f, 9.f, 9.f));
    ASSERT_FLOAT_EQ(go->get_position().x, 9.f);

    lvl.rollback();

    EXPECT_FLOAT_EQ(go->get_position().x, 1.f);
    EXPECT_FLOAT_EQ(go->get_position().y, 2.f);
    EXPECT_FLOAT_EQ(go->get_position().z, 3.f);
}

// An object spawned during play is removed on rollback; the survivor stays.
TEST_F(test_level_rollback, spawned_object_removed)
{
    core::level lvl(AID("rb_lvl_2"));
    auto* survivor = spawn(lvl, "rb_survivor", root::vec3(0.f));
    ASSERT_TRUE(survivor);

    lvl.snapshot();
    auto* spawned = spawn(lvl, "rb_spawned", root::vec3(0.f));
    ASSERT_TRUE(spawned);
    ASSERT_TRUE(lvl.find_game_object(AID("rb_spawned")));

    lvl.rollback();

    EXPECT_FALSE(lvl.find_game_object(AID("rb_spawned")));  // spawned: gone
    EXPECT_TRUE(lvl.find_game_object(AID("rb_survivor")));  // survivor: kept
}

// A survivor destroyed during play is revived on rollback under its SAME id.
TEST_F(test_level_rollback, destroyed_survivor_revived)
{
    core::level lvl(AID("rb_lvl_3"));
    auto* go = spawn(lvl, "rb_doomed", root::vec3(4.f, 5.f, 6.f));
    ASSERT_TRUE(go);

    lvl.snapshot();
    lvl.destroy_game_object(*go);
    ASSERT_FALSE(lvl.find_game_object(AID("rb_doomed")));  // gone during play

    lvl.rollback();

    EXPECT_TRUE(lvl.find_game_object(AID("rb_doomed")));  // revived, same id
}

// A survivor mutated AND THEN destroyed during play comes back with its PRE-PLAY
// value — proving revive composes with the holder-based value restore, not the
// (mutated) destroy-time state.
TEST_F(test_level_rollback, mutated_then_destroyed_survivor_restored)
{
    core::level lvl(AID("rb_lvl_4"));
    auto* go = spawn(lvl, "rb_md", root::vec3(1.f, 1.f, 1.f));
    ASSERT_TRUE(go);

    lvl.snapshot();
    go->set_position(root::vec3(8.f, 8.f, 8.f));  // mutate
    lvl.destroy_game_object(*go);                 // then destroy

    lvl.rollback();

    auto* back = lvl.find_game_object(AID("rb_md"));
    ASSERT_TRUE(back);
    EXPECT_FLOAT_EQ(back->get_position().x, 1.f);  // pre-play, not 8
    EXPECT_FLOAT_EQ(back->get_position().y, 1.f);
    EXPECT_FLOAT_EQ(back->get_position().z, 1.f);
}

// Destroy a survivor AND spawn an object in the same session. destroy_game_object
// reorders m_objects (swap_and_remove), so this exercises the id/set-based rollback
// that replaced the old count/index range: spawned must still be cleaned and the
// destroyed survivor still revived.
TEST_F(test_level_rollback, destroy_and_spawn_together)
{
    core::level lvl(AID("rb_lvl_5"));
    auto* a = spawn(lvl, "rb_a", root::vec3(0.f));
    auto* b = spawn(lvl, "rb_b", root::vec3(0.f));
    ASSERT_TRUE(a && b);

    lvl.snapshot();
    spawn(lvl, "rb_new", root::vec3(0.f));  // spawned during play
    lvl.destroy_game_object(*a);            // survivor destroyed during play

    lvl.rollback();

    EXPECT_TRUE(lvl.find_game_object(AID("rb_a")));     // revived
    EXPECT_TRUE(lvl.find_game_object(AID("rb_b")));     // untouched survivor
    EXPECT_FALSE(lvl.find_game_object(AID("rb_new")));  // spawned: cleaned
}
