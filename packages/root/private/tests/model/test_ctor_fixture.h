#pragma once

#include "testing/testing.h"

#include <core/object_constructor.h>
#include <core/object_load_context.h>
#include <core/level.h>
#include <core/package_manager.h>
#include <core/model_system.h>
#include <core/core_state.h>
#include <core/reflection/reflection_type.h>
#include <global_state/global_state.h>
#include <vfs/vfs_state.h>
#include <vfs/vfs.h>
#include <vfs/physical_backend.h>

#include "packages/root/package.root.h"
#include "packages/root/package.root.types_builder.ar.h"

#include <packages/root/model/game_object.h>
#include <packages/root/model/smart_object.h>
#include <packages/root/model/components/game_object_component.h>
#include <packages/root/model/test/test_root_object.h>
#include <packages/root/model/test/test_root_component.h>

#include <serialization/serialization.h>

#include <gtest/gtest.h>

using namespace kryga;

struct test_ctor : base_test
{
    core::cache_set m_local_cs;
    core::line_cache<root::smart_object_ptr> m_ownable;

    // Instances belong to a level domain, not a package: an instance constructed
    // through a package olc would land its construct()-built sub-objects in the
    // package, which container::unload() forbids. Instance cases construct through
    // this level so they (and their sub-objects) are owned by the level.
    std::unique_ptr<core::level> m_level;

    void
    SetUp() override
    {
        base_test::SetUp();

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

        pm.register_static_package_loader<root::package>();
        auto& pkg = pm.load_static_package<root::package>();
        pkg.init();
        pkg.register_package_extension<root::package::package_types_builder>();
        pkg.load_types();
        pkg.finalize_reflection();
        pkg.set_state(core::package_state::loaded);

        m_level = std::make_unique<core::level>(AID("test_ctor_level"));
        // The level olc needs a vfs mount so instance save/load cases (which
        // resolve + read/write through it) work the same as the package olc.
        m_level->get_load_context().set_vfs_mount(root::package::instance().get_vfs_root());
    }

    void
    TearDown() override
    {
        for (auto& obj : m_ownable)
        {
            glob::glob_state().getr_model().caches.map.remove_item(*obj);
        }
        m_ownable.clear();
        m_local_cs.clear();

        // Free the level (and its instances) before the package: level teardown
        // releases instance objects; the package must then hold only class objects.
        if (m_level)
        {
            m_level->unload();
            m_level.reset();
        }

        root::package::instance().unload();
        glob::glob_state_reset();
        base_test::TearDown();
    }

    core::object_load_context
    make_olc()
    {
        core::object_load_context olc;
        olc.set_package(&root::package::instance())
            .set_local_set(&m_local_cs)
            .set_ownable_cache(&m_ownable)
            .set_vfs_mount(root::package::instance().get_vfs_root());
        return olc;
    }

    // The level's load context — instances built through it carry m_level and
    // own their sub-objects, keeping the package free of instance objects.
    core::object_load_context&
    instance_olc()
    {
        return m_level->get_load_context();
    }

    // Constructor for instance cases: builds through the level so instances carry
    // m_level and their sub-objects land in the level, not the package.
    core::object_constructor
    instance_ctor()
    {
        return core::object_constructor(&instance_olc(),
                                        core::object_load_type::instance_obj);
    }

    // Materialize a type's class object (CDO) in the PACKAGE up front. Production
    // loads CDOs at package-load time; here construction is lazy, and an instance
    // built through the level would otherwise create the CDO via the level olc —
    // leaving it domainless (no package, no level). Pre-creating it in the package
    // keeps CDOs where they belong; the level instance then finds it in the cache.
    void
    ensure_package_cdo(const utils::id& type_id)
    {
        auto& plc = root::package::instance().get_load_context();
        if (plc.find_obj(type_id))
        {
            return;
        }
        auto* rt = glob::glob_state().getr_model().reflection.get_type(type_id);
        core::object_constructor(&plc).create_default_class_obj_impl(rt);
    }

    reflection::reflection_type*
    get_rt(const char* name)
    {
        return glob::glob_state().getr_model().reflection.get_type(AID(name));
    }

    void
    expect_flags_cdo(root::smart_object* obj)
    {
        EXPECT_TRUE(obj->get_flags().default_obj) << obj->get_id();
        EXPECT_TRUE(obj->get_flags().readonly) << obj->get_id();
        EXPECT_FALSE(obj->get_flags().instance_obj) << obj->get_id();
    }

    void
    expect_flags_proto(root::smart_object* obj)
    {
        EXPECT_TRUE(obj->get_flags().derived_obj) << obj->get_id();
        EXPECT_TRUE(obj->get_flags().readonly) << obj->get_id();
        EXPECT_FALSE(obj->get_flags().instance_obj) << obj->get_id();
    }

    void
    expect_flags_instance(root::smart_object* obj)
    {
        EXPECT_TRUE(obj->get_flags().instance_obj) << obj->get_id();
        EXPECT_FALSE(obj->get_flags().readonly) << obj->get_id();
        EXPECT_TRUE(obj->get_flags().derived_obj) << obj->get_id();
    }

    void
    expect_in_cache(root::smart_object* obj)
    {
        EXPECT_EQ(glob::glob_state().getr_model().caches.objects.get_item(obj->get_id()), obj)
            << obj->get_id();
    }

    void
    expect_obj(root::smart_object* obj, const utils::id& expected_id)
    {
        ASSERT_NE(obj, nullptr) << expected_id;
        EXPECT_EQ(obj->get_id(), expected_id);
        expect_in_cache(obj);
    }
};

// Property result by operation × inst_mode:
//
// ┌────────────────────┬───────────────┬───────────────┐
// │                    │ share         │ instantiate    │
// ├────────────────────┼───────────────┼───────────────┤
// │ construct proto    │ shared (ro)   │ new obj (ro)   │
// │ construct instance │ shared (ro)   │ new obj (inst) │
// │ instantiate        │ shared        │ new obj (inst) │
// └────────────────────┴───────────────┴───────────────┘
//
