
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
#include "packages/base/package.base.h"
#include "packages/test/package.test.h"

#include <packages/root/model/game_object.h>
#include <packages/root/model/assets/mesh.h>
#include <packages/root/model/assets/material.h>
#include <packages/root/model/assets/texture.h>
#include <packages/test/model/test_mesh_object.h>
#include <packages/base/model/components/mesh_component.h>
#include <packages/base/model/assets/simple_texture_material.h>

#include <utils/agea_log.h>
#include <utils/file_utils.h>

#include <gtest/gtest.h>
#include <sstream>

using namespace agea;

namespace
{

void
validate_empty_cache(gs::state& gs)
{
    for (auto i = core::architype::first; i < core::architype::last;
         i = (core::architype)((uint8_t)i + 1))
    {
        ASSERT_TRUE(gs.getr_class_cache_map().get_cache(i)->get_items().empty())
            << "Failed at " << ::agea::core::to_string(i);
    }
}

}  // namespace

struct test_preloaded_test_package : base_test
{
    void
    SetUp()
    {
        base::package::reset_instance();
        root::package::reset_instance();
        test::package::reset_instance();
        glob::glob_state_reset();

        auto& gs = glob::glob_state();

        ///
        gs.schedule_action(gs::state::state_stage::create,
                           [](gs::state& s)
                           {
                               // state
                               core::state_mutator__caches::set(s);
                               core::state_mutator__level_manager::set(s);
                               core::state_mutator__package_manager::set(s);
                               core::state_mutator__reflection_manager::set(s);
                               core::state_mutator__id_generator::set(s);
                               core::state_mutator__lua_api::set(s);
                               state_mutator__resource_locator::set(s);
                           });
        gs.run_create();
        validate_empty_cache(gs);
        {
            {
                root::package::init_instance();
                auto& pkg = root::package::instance();
                pkg.register_package_extention<root::package::package_types_builder>();
                pkg.complete_load();
            }
            {
                base::package::init_instance();
                auto& pkg = base::package::instance();
                pkg.register_package_extention<base::package::package_types_builder>();
                pkg.complete_load();
            }

            {
                test::package::init_instance();
                auto& pkg = test::package::instance();
                pkg.init();

                auto tb = (test::package::package_types_builder*)test::package::instance()
                              .types_builder()
                              .get();

                pkg.finalize_relfection();
            }
        }
    }

    void
    TearDown()
    {
        test::package::instance().complete_unload();
        test::package::reset_instance();
        base::package::instance().complete_unload();
        base::package::reset_instance();
        root::package::instance().complete_unload();
        root::package::reset_instance();
        glob::glob_state_reset();

        base_test::TearDown();
    }

    testing::AssertionResult
    validate_class_obj(root::smart_object& so)
    {
        auto class_obj = so.get_class_obj();
        if ((so.get_flags().derived_obj || so.get_flags().instance_obj) &&
            !so.get_flags().runtime_obj && !class_obj)
        {
            return testing::AssertionFailure() << "get_class_obj() is null for " << so.get_id();
        }

        if ((so.get_flags().default_obj || so.get_flags().runtime_obj) && class_obj)
        {
            return testing::AssertionFailure() << "get_class_obj() is not null for " << so.get_id();
        }

        return testing::AssertionSuccess();
    }

    testing::AssertionResult
    verify_flags(root::smart_object& so, const root::smart_object_flags& expected_flags)
    {
        const auto& flags = so.get_flags();
        std::stringstream errors;

        if (flags.instance_obj != expected_flags.instance_obj)
        {
            errors << "instance_obj flag mismatch: expected " << expected_flags.instance_obj
                   << " but got " << flags.instance_obj << " for " << so.get_id() << "; ";
        }

        if (flags.derived_obj != expected_flags.derived_obj)
        {
            errors << "derived_obj flag mismatch: expected " << expected_flags.derived_obj
                   << " but got " << flags.derived_obj << " for " << so.get_id() << "; ";
        }

        if (flags.runtime_obj != expected_flags.runtime_obj)
        {
            errors << "runtime_constructed flag mismatch: expected " << expected_flags.runtime_obj
                   << " but got " << flags.runtime_obj << " for " << so.get_id() << "; ";
        }

        if (flags.mirror_obj != expected_flags.mirror_obj)
        {
            errors << "mirror_obj flag mismatch: expected " << expected_flags.mirror_obj
                   << " but got " << flags.mirror_obj << " for " << so.get_id() << "; ";
        }

        std::string error_msg = errors.str();
        if (!error_msg.empty())
        {
            return testing::AssertionFailure() << error_msg;
        }

        return testing::AssertionSuccess();
    }
};

TEST_F(test_preloaded_test_package, load_class_object_with_custom_layout)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    lc.set_prefix_path(obj_path);
    lc.get_objects_mapping()
        .add(AID("test_mesh"), false, APATH("game_objects/test_mesh.aobj"))
        .add(AID("test_material"), false, APATH("game_objects/test_material.aobj"))
        .add(AID("test_complex_mesh_object"), false,
             APATH("game_objects/test_complex_mesh_object.aobj"));

    std::vector<root::smart_object*> loaded;
    auto result = core::object_constructor::object_load(
        AID("test_complex_mesh_object"), core::object_load_type::class_obj, lc, loaded);

    ASSERT_TRUE(result.has_value());
    auto go = result.value()->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_TRUE(verify_flags(*go, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*go));

    ASSERT_EQ(loaded.size(), 5);

    ASSERT_EQ(go->get_id(), AID("test_complex_mesh_object"));
    ASSERT_EQ(go->get_architype_id(), core::architype::game_object);
    ASSERT_EQ(go->get_class_obj()->get_id(), AID("mesh_object"));

    auto components = go->get_subcomponents();
    ASSERT_EQ(components.size(), 2);

    auto comp1 = components[0];
    ASSERT_EQ(comp1->get_id(), AID("test_root_component_0"));
    ASSERT_EQ(comp1->get_architype_id(), core::architype::component);
    ASSERT_EQ(comp1->get_class_obj()->get_id(), AID("game_object_component"));
    ASSERT_TRUE(verify_flags(*comp1, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*comp1));

    auto comp2 = components[1]->as<base::mesh_component>();
    ASSERT_EQ(comp2->get_id(), AID("test_root_component_1"));
    ASSERT_EQ(comp2->get_architype_id(), core::architype::component);
    ASSERT_EQ(comp2->get_class_obj()->get_id(), AID("mesh_component"));
    ASSERT_TRUE(verify_flags(*comp2, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*comp2));

    auto mesh = comp2->get_mesh();
    ASSERT_EQ(mesh->get_id(), AID("test_mesh"));
    ASSERT_EQ(mesh->get_class_obj()->get_id(), AID("mesh"));
    ASSERT_TRUE(verify_flags(*mesh, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*mesh));

    auto material = comp2->get_material()->as<base::simple_texture_material>();
    ASSERT_EQ(material->get_id(), AID("test_material"));
    ASSERT_EQ(material->get_class_obj()->get_id(), AID("simple_texture_material"));
    ASSERT_TRUE(verify_flags(*material, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*material));

    auto& ts = material->get_sample(AID("simple_texture"));

    ASSERT_EQ(ts.sampler_id, AID("default"));
    ASSERT_EQ(ts.slot, 0);
    ASSERT_TRUE(verify_flags(*ts.txt, core::ks_class_constructed));
    ASSERT_TRUE(validate_class_obj(*ts.txt));

    ASSERT_EQ(ts.txt->get_id(), AID("texture"));
}

TEST_F(test_preloaded_test_package, load_class_object_by_id)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    lc.set_prefix_path(obj_path);
    lc.get_objects_mapping().add(AID("test_obj"), false, APATH("game_objects/test_obj.aobj"));

    std::vector<root::smart_object*> loaded;
    auto result = core::object_constructor::object_load(
        AID("test_obj"), core::object_load_type::class_obj, lc, loaded);

    ASSERT_TRUE(result.has_value());
    auto go = result.value()->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_TRUE(verify_flags(*go, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*go));

    ASSERT_EQ(go->get_id(), AID("test_obj"));
    ASSERT_EQ(go->get_class_obj()->get_id(), AID("game_object"));
}

TEST_F(test_preloaded_test_package, load_instance_object_by_id)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    lc.set_prefix_path(obj_path);
    lc.get_objects_mapping().add(AID("test_obj"), false, APATH("game_objects/test_obj.aobj"));

    std::vector<root::smart_object*> loaded;
    auto result = core::object_constructor::object_load(
        AID("test_obj"), core::object_load_type::instance_obj, lc, loaded);

    ASSERT_TRUE(result.has_value());
    auto go = result.value()->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_TRUE(verify_flags(*go, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*go));

    ASSERT_EQ(go->get_id(), AID("test_obj"));
}

TEST_F(test_preloaded_test_package, object_clone_class_object)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    lc.set_prefix_path(obj_path);
    lc.get_objects_mapping().add(AID("test_obj"), false, APATH("game_objects/test_obj.aobj"));

    std::vector<root::smart_object*> loaded;
    auto load_result = core::object_constructor::object_load(
        AID("test_obj"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());
    auto src = load_result.value();
    ASSERT_TRUE(verify_flags(*src, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*src));

    std::vector<root::smart_object*> cloned_objs;
    auto clone_result = core::object_constructor::object_clone(
        *src, core::object_load_type::class_obj, AID("test_obj_clone"), lc, cloned_objs);

    ASSERT_TRUE(clone_result.has_value());
    auto cloned = clone_result.value();
    ASSERT_NE(cloned, src);
    ASSERT_EQ(cloned->get_id(), AID("test_obj_clone"));
    ASSERT_EQ(cloned->get_type_id(), src->get_type_id());
    ASSERT_TRUE(verify_flags(*cloned, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*cloned));
}

TEST_F(test_preloaded_test_package, object_clone_as_instance)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    lc.set_prefix_path(obj_path);
    lc.get_objects_mapping().add(AID("test_obj"), false, APATH("game_objects/test_obj.aobj"));

    std::vector<root::smart_object*> loaded;
    auto load_result = core::object_constructor::object_load(
        AID("test_obj"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());
    auto src = load_result.value();
    ASSERT_TRUE(verify_flags(*src, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*src));

    std::vector<root::smart_object*> cloned_objs;
    auto clone_result =
        core::object_constructor::object_clone(*src, core::object_load_type::instance_obj,
                                               AID("test_obj_instance_clone"), lc, cloned_objs);

    ASSERT_TRUE(clone_result.has_value());
    auto cloned = clone_result.value();
    ASSERT_NE(cloned, src);
    ASSERT_EQ(cloned->get_id(), AID("test_obj_instance_clone"));
    ASSERT_TRUE(verify_flags(*cloned, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*cloned));
}

TEST_F(test_preloaded_test_package, object_instantiate_from_proto)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    lc.set_prefix_path(obj_path);
    lc.get_objects_mapping().add(AID("test_obj"), false, APATH("game_objects/test_obj.aobj"));

    std::vector<root::smart_object*> loaded;
    auto load_result = core::object_constructor::object_load(
        AID("test_obj"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());

    auto proto = load_result.value();
    ASSERT_TRUE(validate_class_obj(*proto));

    std::vector<root::smart_object*> instantiated_objs;
    auto inst_result = core::object_constructor::object_instantiate(
        *proto, AID("test_obj_instance"), lc, instantiated_objs);

    ASSERT_TRUE(inst_result.has_value());
    auto instance = inst_result.value();
    ASSERT_NE(instance, proto);
    ASSERT_EQ(instance->get_id(), AID("test_obj_instance"));
    ASSERT_EQ(instance->get_class_obj(), proto);
    ASSERT_TRUE(verify_flags(*instance, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*instance));
}

TEST_F(test_preloaded_test_package, diff_object_properties_same_objects)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    lc.set_prefix_path(obj_path);
    lc.get_objects_mapping().add(AID("test_obj"), false, APATH("game_objects/test_obj.aobj"));

    std::vector<root::smart_object*> loaded1;
    auto result1 = core::object_constructor::object_load(
        AID("test_obj"), core::object_load_type::class_obj, lc, loaded1);
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(verify_flags(*result1.value(), {.instance_obj = false, .derived_obj = true}));
    ASSERT_TRUE(validate_class_obj(*result1.value()));

    std::vector<reflection::property*> diff;
    auto rc =
        core::object_constructor::diff_object_properties(*result1.value(), *result1.value(), diff);

    ASSERT_EQ(rc, result_code::ok);
    ASSERT_TRUE(diff.empty());
}

TEST_F(test_preloaded_test_package, diff_object_properties_different_types_fails)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    lc.set_prefix_path(obj_path);
    lc.get_objects_mapping()
        .add(AID("test_obj"), false, APATH("game_objects/test_obj.aobj"))
        .add(AID("test_mesh"), false, APATH("game_objects/test_mesh.aobj"));

    std::vector<root::smart_object*> loaded1, loaded2;
    auto result1 = core::object_constructor::object_load(
        AID("test_obj"), core::object_load_type::class_obj, lc, loaded1);
    auto result2 = core::object_constructor::object_load(
        AID("test_mesh"), core::object_load_type::class_obj, lc, loaded2);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_TRUE(validate_class_obj(*result1.value()));
    ASSERT_TRUE(validate_class_obj(*result2.value()));

    std::vector<reflection::property*> diff;
    auto rc =
        core::object_constructor::diff_object_properties(*result1.value(), *result2.value(), diff);

    ASSERT_EQ(rc, result_code::failed);
}

TEST_F(test_preloaded_test_package, load_nonexistent_object_fails)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    lc.set_prefix_path(obj_path);

    std::vector<root::smart_object*> loaded;
    auto result = core::object_constructor::object_load(
        AID("nonexistent_object"), core::object_load_type::class_obj, lc, loaded);

    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error(), result_code::path_not_found);
}

TEST_F(test_preloaded_test_package, load_invalid_path_fails)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    lc.set_prefix_path(obj_path);
    lc.get_objects_mapping().add(AID("does_not_exist"), false,
                                 APATH("game_objects/does_not_exist.aobj"));

    std::vector<root::smart_object*> loaded;
    auto result = core::object_constructor::object_load(
        AID("does_not_exist"), core::object_load_type::class_obj, lc, loaded);

    ASSERT_FALSE(result.has_value());
}

TEST_F(test_preloaded_test_package, cached_object_returns_same_pointer)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    lc.set_prefix_path(obj_path);
    lc.get_objects_mapping().add(AID("test_obj"), false, APATH("game_objects/test_obj.aobj"));

    std::vector<root::smart_object*> loaded1, loaded2;
    auto result1 = core::object_constructor::object_load(
        AID("test_obj"), core::object_load_type::class_obj, lc, loaded1);
    auto result2 = core::object_constructor::object_load(
        AID("test_obj"), core::object_load_type::class_obj, lc, loaded2);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_EQ(result1.value(), result2.value());
    ASSERT_TRUE(verify_flags(*result1.value(), {.instance_obj = false, .derived_obj = true}));
    ASSERT_TRUE(validate_class_obj(*result1.value()));
}

TEST_F(test_preloaded_test_package, object_instantiate_complex_object_with_components)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    lc.set_prefix_path(obj_path);
    lc.get_objects_mapping()
        .add(AID("test_mesh"), false, APATH("game_objects/test_mesh.aobj"))
        .add(AID("test_material"), false, APATH("game_objects/test_material.aobj"))
        .add(AID("test_complex_mesh_object"), false,
             APATH("game_objects/test_complex_mesh_object.aobj"));

    std::vector<root::smart_object*> loaded;
    auto load_result = core::object_constructor::object_load(
        AID("test_complex_mesh_object"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());

    auto proto = load_result.value()->as<root::game_object>();
    ASSERT_TRUE(proto);
    ASSERT_TRUE(validate_class_obj(*proto));

    std::vector<root::smart_object*> instantiated_objs;
    auto inst_result = core::object_constructor::object_instantiate(*proto, AID("complex_instance"),
                                                                    lc, instantiated_objs);

    ASSERT_TRUE(inst_result.has_value());
    auto instance = inst_result.value()->as<root::game_object>();
    ASSERT_TRUE(instance);
    ASSERT_TRUE(verify_flags(*instance, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*instance));

    ASSERT_EQ(instance->get_id(), AID("complex_instance"));

    auto proto_components = proto->get_subcomponents();
    auto instance_components = instance->get_subcomponents();
    ASSERT_EQ(instance_components.size(), proto_components.size());

    for (auto comp : instance_components)
    {
        ASSERT_TRUE(verify_flags(*comp, core::ks_instance_derived));
        ASSERT_TRUE(validate_class_obj(*comp));
    }
}

TEST_F(test_preloaded_test_package, load_instance_object_with_custom_layout)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    lc.set_prefix_path(obj_path);
    std::vector<root::smart_object*> loaded;
    lc.get_objects_mapping()
        .add(AID("test_mesh"), false, APATH("game_objects/test_mesh.aobj"))
        .add(AID("test_material"), false, APATH("game_objects/test_material.aobj"))
        .add(AID("test_complex_mesh_object"), false,
             APATH("game_objects/test_complex_mesh_object.aobj"));

    auto result = core::object_constructor::object_load(
        AID("test_complex_mesh_object"), core::object_load_type::instance_obj, lc, loaded);

    ASSERT_TRUE(result.has_value());
    auto go = result.value()->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_TRUE(verify_flags(*go, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*go));

    ASSERT_EQ(go->get_id(), AID("test_complex_mesh_object"));
    ASSERT_EQ(go->get_architype_id(), core::architype::game_object);

    auto components = go->get_subcomponents();
    ASSERT_EQ(components.size(), 2);

    auto comp1 = components[0];
    ASSERT_EQ(comp1->get_id(), AID("test_root_component_0"));
    ASSERT_EQ(comp1->get_architype_id(), core::architype::component);
    ASSERT_EQ(comp1->get_class_obj()->get_id(), AID("test_root_component_0"));
    ASSERT_TRUE(verify_flags(*comp1, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*comp1));

    auto comp2 = components[1]->as<base::mesh_component>();
    ASSERT_EQ(comp2->get_id(), AID("test_root_component_1"));
    ASSERT_EQ(comp2->get_architype_id(), core::architype::component);
    ASSERT_EQ(comp2->get_class_obj()->get_id(), AID("test_root_component_1"));
    ASSERT_TRUE(verify_flags(*comp2, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*comp2));

    auto mesh = comp2->get_mesh();
    ASSERT_EQ(mesh->get_id(), AID("test_mesh"));
    ASSERT_EQ(mesh->get_class_obj()->get_id(), AID("test_mesh"));
    ASSERT_TRUE(verify_flags(*mesh, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*mesh));

    auto material = comp2->get_material()->as<base::simple_texture_material>();
    ASSERT_EQ(material->get_id(), AID("test_material"));
    ASSERT_EQ(material->get_class_obj()->get_id(), AID("test_material"));
    ASSERT_TRUE(verify_flags(*material, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*material));

    auto& ts = material->get_sample(AID("simple_texture"));

    ASSERT_EQ(ts.sampler_id, AID("default"));
    ASSERT_EQ(ts.slot, 0);
    ASSERT_TRUE(verify_flags(*ts.txt, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*ts.txt));

    ASSERT_EQ(ts.txt->get_id(), AID("texture"));
}

TEST_F(test_preloaded_test_package, object_construct_in_package_context)
{
    auto& lc = test::package::instance().get_load_context();

    root::game_object::construct_params params;
    params.pos = {1.0f, 2.0f, 3.0f};

    auto result = core::object_constructor::object_construct(
        AID("game_object"), AID("constructed_game_object"), params, lc);

    ASSERT_TRUE(result.has_value());
    auto obj = result.value();
    ASSERT_TRUE(obj);

    ASSERT_EQ(obj->get_id(), AID("constructed_game_object"));
    ASSERT_EQ(obj->get_type_id(), AID("game_object"));

    // Package context creates proto objects
    ASSERT_TRUE(verify_flags(*obj, core::ks_class_constructed));
    ASSERT_TRUE(validate_class_obj(*obj));

    auto go = obj->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_EQ(go->get_position(), root::vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(test_preloaded_test_package, object_construct_invalid_type_fails)
{
    auto& lc = test::package::instance().get_load_context();

    root::smart_object::construct_params params;

    auto result = core::object_constructor::object_construct(AID("nonexistent_type_xyz"),
                                                             AID("should_fail"), params, lc);

    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error(), result_code::id_not_found);
}

TEST_F(test_preloaded_test_package, object_construct_in_level_context)
{
    // Create a level - its constructor sets up load context automatically
    core::level test_level(AID("test_construct_level"));

    auto& lc = test_level.get_load_context();

    root::game_object::construct_params params;
    params.pos = {5.0f, 6.0f, 7.0f};

    auto result = core::object_constructor::object_construct(
        AID("game_object"), AID("level_constructed_object"), params, lc);

    ASSERT_TRUE(result.has_value());
    auto obj = result.value();
    ASSERT_TRUE(obj);

    ASSERT_EQ(obj->get_id(), AID("level_constructed_object"));
    ASSERT_EQ(obj->get_type_id(), AID("game_object"));

    // Level context creates instance objects
    ASSERT_TRUE(verify_flags(*obj, core::ks_instance_constructed));
    ASSERT_TRUE(validate_class_obj(*obj));

    auto go = obj->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_EQ(go->get_position(), root::vec3(5.0f, 6.0f, 7.0f));
}

// ============================================================================
// object_save round-trip tests
// ============================================================================

TEST_F(test_preloaded_test_package, DISABLED_object_save_and_reload_full)
{
    auto& lc = test::package::instance().get_load_context();

    // 1. Construct an object (same pattern as working test)
    root::game_object::construct_params params;
    params.pos = {10.0f, 20.0f, 30.0f};

    auto construct_result = core::object_constructor::object_construct(
        AID("game_object"), AID("save_test_object"), params, lc);
    ASSERT_TRUE(construct_result.has_value());

    auto obj = construct_result.value();
    ASSERT_TRUE(obj);

    auto original = obj->as<root::game_object>();
    ASSERT_TRUE(original);
    ASSERT_TRUE(verify_flags(*original, {.instance_obj = false, .derived_obj = true}));
    ASSERT_TRUE(validate_class_obj(*original));

    // 2. Save to a temp file
    auto temp_dir = utils::path(std::filesystem::temp_directory_path());
    auto save_path = temp_dir / "test_save_object.aobj";

    auto save_result = core::object_constructor::object_save(*original, save_path);
    ASSERT_EQ(save_result, result_code::ok);
    ASSERT_TRUE(save_path.exists());

    // 3. Load it back using a fresh load context
    core::level reload_level(AID("reload_test_level"));
    auto& reload_lc = reload_level.get_load_context();
    reload_lc.set_prefix_path(temp_dir);
    reload_lc.get_objects_mapping().add(AID("save_test_object"), false,
                                        APATH("test_save_object.aobj"));

    std::vector<root::smart_object*> loaded_objs;
    auto load_result = core::object_constructor::object_load(
        AID("save_test_object"), core::object_load_type::instance_obj, reload_lc, loaded_objs);

    ASSERT_TRUE(load_result.has_value());
    auto reloaded = load_result.value()->as<root::game_object>();
    ASSERT_TRUE(reloaded);
    ASSERT_TRUE(verify_flags(*reloaded, {.instance_obj = true, .derived_obj = true}));
    ASSERT_TRUE(validate_class_obj(*reloaded));

    // 4. Verify basic properties match
    ASSERT_EQ(reloaded->get_id(), AID("save_test_object"));
    ASSERT_EQ(reloaded->get_type_id(), AID("game_object"));

    // Cleanup
    std::filesystem::remove(save_path.fs());
}

TEST_F(test_preloaded_test_package, DISABLED_object_save_and_reload_partial_inherited)
{
    auto& lc = test::package::instance().get_load_context();

    // 1. Create a proto object first (same pattern as working test)
    root::game_object::construct_params proto_params;
    proto_params.pos = {0.0f, 0.0f, 0.0f};

    auto proto_result = core::object_constructor::object_construct(
        AID("game_object"), AID("inherited_proto"), proto_params, lc);
    ASSERT_TRUE(proto_result.has_value());

    auto proto_obj = proto_result.value();
    ASSERT_TRUE(proto_obj);

    auto proto = proto_obj->as<root::game_object>();
    ASSERT_TRUE(proto);
    ASSERT_TRUE(verify_flags(*proto, {.instance_obj = false, .derived_obj = true}));
    ASSERT_TRUE(validate_class_obj(*proto));

    // 2. Instantiate from proto
    std::vector<root::smart_object*> instantiated_objs;
    auto inst_result = core::object_constructor::object_instantiate(
        *proto, AID("inherited_instance"), lc, instantiated_objs);
    ASSERT_TRUE(inst_result.has_value());

    auto inst_obj = inst_result.value();
    ASSERT_TRUE(inst_obj);

    auto instance = inst_obj->as<root::game_object>();
    ASSERT_TRUE(instance);
    ASSERT_EQ(instance->get_class_obj(), proto);
    ASSERT_TRUE(verify_flags(*instance, {.instance_obj = true, .derived_obj = true}));
    ASSERT_TRUE(validate_class_obj(*instance));

    // 3. Set inhereted flag to use partial save path
    // instance->get_flags().inhereted = true;

    // 4. Save to temp file
    auto temp_dir = utils::path(std::filesystem::temp_directory_path());
    auto save_path = temp_dir / "test_inherited_object.aobj";

    auto save_result = core::object_constructor::object_save(*instance, save_path);
    ASSERT_EQ(save_result, result_code::ok);
    ASSERT_TRUE(save_path.exists());

    // 5. Load it back
    core::level reload_level(AID("reload_inherited_level"));
    auto& reload_lc = reload_level.get_load_context();
    reload_lc.set_prefix_path(temp_dir);
    reload_lc.get_objects_mapping().add(AID("inherited_instance"), false,
                                        APATH("test_inherited_object.aobj"));

    std::vector<root::smart_object*> loaded_objs;
    auto load_result = core::object_constructor::object_load(
        AID("inherited_instance"), core::object_load_type::instance_obj, reload_lc, loaded_objs);

    ASSERT_TRUE(load_result.has_value());
    auto reloaded = load_result.value()->as<root::game_object>();
    ASSERT_TRUE(reloaded);
    ASSERT_TRUE(verify_flags(*reloaded, {.instance_obj = true, .derived_obj = true}));
    ASSERT_TRUE(validate_class_obj(*reloaded));

    // 6. Verify basic properties
    ASSERT_EQ(reloaded->get_id(), AID("inherited_instance"));

    // Cleanup
    std::filesystem::remove(save_path.fs());
}
