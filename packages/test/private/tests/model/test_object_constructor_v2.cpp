
#include "core/object_constructor.h"

#include <core/package_manager.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <global_state/global_state.h>
#include <core/reflection/reflection_type.h>
#include <core/architype.h>
#include <core/object_load_context.h>
#include <testing/testing.h>
#include <core/core_state.h>
#include <resource_locator/resource_locator_state.h>

#include "packages/root/package.root.h"
#include "packages/root/package.root.types_builder.ar.h"
#include "packages/base/package.base.h"
#include "packages/base/package.base.types_builder.ar.h"
#include "packages/test/package.test.h"

#include <packages/root/model/game_object.h>
#include <packages/root/model/assets/mesh.h>
#include <packages/root/model/assets/material.h>
#include <packages/base/model/components/mesh_component.h>
#include <packages/base/model/assets/simple_texture_material.h>

#include <utils/kryga_log.h>

#include <gtest/gtest.h>

using namespace kryga;

struct test_object_constructor : base_test
{
    void
    SetUp()
    {
        glob::glob_state_reset();

        auto& gs = glob::glob_state();
        core::state_mutator__id_generator::set(gs);
        state_mutator__resource_locator::set(gs);
        core::state_mutator__caches::set(gs);
        core::state_mutator__reflection_manager::set(gs);
        core::state_mutator__lua_api::set(gs);
        core::state_mutator__package_manager::set(gs);
        auto& pm = gs.getr_pm();

        gs.schedule_action(gs::state::state_stage::create,
                           [](gs::state& s) { core::state_mutator__level_manager::set(s); });
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

    void
    setup_test_level_path(core::object_load_context& lc)
    {
        auto& gs = glob::glob_state();
        auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
        lc.set_prefix_path(obj_path);
    }
};

TEST_F(test_object_constructor, load_simple_class_object)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_level_path(lc);
    lc.get_objects_mapping().add(AID("test_obj"), false, APATH("game_objects/test_obj.aobj"));

    core::object_constructor ctor(&lc);
    auto result = ctor.load_package_obj(AID("test_obj"));

    ASSERT_TRUE(result.has_value());
    auto go = result.value()->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_EQ(go->get_id(), AID("test_obj"));
    ASSERT_EQ(go->get_class_obj()->get_id(), AID("game_object"));
}

TEST_F(test_object_constructor, load_returns_cached_on_second_call)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_level_path(lc);
    lc.get_objects_mapping().add(AID("test_obj"), false, APATH("game_objects/test_obj.aobj"));

    core::object_constructor ctor(&lc);
    auto result1 = ctor.load_package_obj(AID("test_obj"));
    auto result2 = ctor.load_package_obj(AID("test_obj"));

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_EQ(result1.value(), result2.value());
}

TEST_F(test_object_constructor, load_nonexistent_object_fails)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_level_path(lc);

    core::object_constructor ctor(&lc);
    auto result = ctor.load_package_obj(AID("nonexistent_object"));

    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error(), result_code::path_not_found);
}

TEST_F(test_object_constructor, load_invalid_path_fails)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_level_path(lc);
    lc.get_objects_mapping().add(AID("does_not_exist"), false,
                                 APATH("game_objects/does_not_exist.aobj"));

    core::object_constructor ctor(&lc);
    auto result = ctor.load_package_obj(AID("does_not_exist"));

    ASSERT_FALSE(result.has_value());
}

TEST_F(test_object_constructor, load_default_class_object_by_type_id)
{
    auto& lc = test::package::instance().get_load_context();

    core::object_constructor ctor(&lc);
    auto result = ctor.load_package_obj(AID("game_object"));

    ASSERT_TRUE(result.has_value());
    auto obj = result.value();
    ASSERT_EQ(obj->get_id(), AID("game_object"));
    ASSERT_TRUE(obj->get_flags().default_obj);
}

TEST_F(test_object_constructor, load_default_class_returns_cached)
{
    auto& lc = test::package::instance().get_load_context();

    core::object_constructor ctor(&lc);
    auto result1 = ctor.load_package_obj(AID("game_object"));
    auto result2 = ctor.load_package_obj(AID("game_object"));

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_EQ(result1.value(), result2.value());
}

TEST_F(test_object_constructor, load_complex_object_with_dependencies)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_level_path(lc);
    lc.get_objects_mapping()
        .add(AID("test_mesh"), false, APATH("game_objects/test_mesh.aobj"))
        .add(AID("test_material"), false, APATH("game_objects/test_material.aobj"))
        .add(AID("test_complex_mesh_object"), false,
             APATH("game_objects/test_complex_mesh_object.aobj"));

    core::object_constructor ctor(&lc);
    auto result = ctor.load_package_obj(AID("test_complex_mesh_object"));

    ASSERT_TRUE(result.has_value());
    auto go = result.value()->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_EQ(go->get_id(), AID("test_complex_mesh_object"));
    ASSERT_EQ(go->get_architype_id(), core::architype::game_object);

    auto components = go->get_subcomponents();
    ASSERT_EQ(components.size(), 2);

    auto comp2 = components[1]->as<base::mesh_component>();
    ASSERT_TRUE(comp2);

    auto mesh = comp2->get_mesh();
    ASSERT_TRUE(mesh);
    ASSERT_EQ(mesh->get_id(), AID("test_mesh"));

    auto material = comp2->get_material()->as<base::simple_texture_material>();
    ASSERT_TRUE(material);
    ASSERT_EQ(material->get_id(), AID("test_material"));
}

TEST_F(test_object_constructor, load_produces_class_flags)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_level_path(lc);
    lc.get_objects_mapping().add(AID("test_obj"), false, APATH("game_objects/test_obj.aobj"));

    core::object_constructor ctor(&lc);
    auto result = ctor.load_package_obj(AID("test_obj"));

    ASSERT_TRUE(result.has_value());
    auto obj = result.value();
    auto& flags = obj->get_flags();
    ASSERT_FALSE(flags.instance_obj);
    ASSERT_TRUE(flags.derived_obj);
    ASSERT_FALSE(flags.runtime_obj);
}

TEST_F(test_object_constructor, load_sets_package)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_level_path(lc);
    lc.get_objects_mapping().add(AID("test_obj"), false, APATH("game_objects/test_obj.aobj"));

    core::object_constructor ctor(&lc);
    auto result = ctor.load_package_obj(AID("test_obj"));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value()->get_package(), lc.get_package());
}
