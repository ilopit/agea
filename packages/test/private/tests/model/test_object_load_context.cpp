
#include "core/object_load_context.h"

#include <core/model_system.h>
#include <core/object_constructor.h>
#include <core/package_manager.h>
#include <core/architype.h>
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

#include <packages/root/model/smart_object.h>
#include <packages/root/model/game_object.h>
#include <packages/root/model/assets/mesh.h>
#include <packages/root/model/assets/material.h>
#include <packages/root/model/assets/texture.h>
#include <packages/root/model/components/component.h>
#include <packages/base/model/components/mesh_component.h>
#include <packages/base/model/assets/simple_texture_material.h>

#include <gtest/gtest.h>

using namespace kryga;

struct test_olc : base_test
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
        auto& pm = gs.getr_model().packages;

        gs.run_create();
        {
            pm.register_static_package_loader<root::package>();
            auto& pkg = pm.load_static_package<root::package>();
            pkg.init();
            pkg.register_package_extension<root::package::package_types_builder>();
            pkg.complete_load();
        }
        {
            pm.register_static_package_loader<base::package>();
            auto& pkg = pm.load_static_package<base::package>();
            pkg.init();
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
};

// ============================================================================
// add_obj: class objects enter the cache
// ============================================================================

TEST_F(test_olc, add_class_obj_enters_global_cache)
{
}

TEST_F(test_olc, add_class_obj_enters_local_cache)
{
}

TEST_F(test_olc, add_class_obj_duplicate_id_asserts)
{
}

// ============================================================================
// add_obj: instance objects — lifetime only, no cache
// ============================================================================

TEST_F(test_olc, add_instance_obj_does_not_enter_global_cache)
{
}

TEST_F(test_olc, add_instance_obj_stays_alive_via_ownable_cache)
{
}

// ============================================================================
// find_obj
// ============================================================================

TEST_F(test_olc, find_obj_returns_class_object)
{
}

TEST_F(test_olc, find_obj_checks_local_then_global)
{
}

TEST_F(test_olc, find_obj_by_architype)
{
}

// ============================================================================
// remove_obj
// ============================================================================

TEST_F(test_olc, remove_class_obj_removes_from_both_caches)
{
}

// ============================================================================
// resolve
// ============================================================================

TEST_F(test_olc, resolve_by_id_with_mounted_vfs)
{
}

TEST_F(test_olc, resolve_by_path_with_mounted_vfs)
{
}

TEST_F(test_olc, resolve_fails_without_vfs_mount)
{
}
