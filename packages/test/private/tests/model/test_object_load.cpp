
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
        m_object_maping = std::make_shared<core::object_mapping>();
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
            root::package::init_instance();
            auto& pkg = root::package::instance();
            pkg.register_package_extention<root::package::package_types_builder>();
            pkg.register_package_extention<root::package::package_types_default_objects_builder>();
            pkg.complete_load();
        }
        {
            base::package::init_instance();
            auto& pkg = base::package::instance();
            pkg.register_package_extention<base::package::package_types_builder>();
            pkg.register_package_extention<base::package::package_types_default_objects_builder>();
            pkg.complete_load();
        }
        {
            test::package::init_instance();
            auto& pkg = test::package::instance();
            pkg.register_package_extention<test::package::package_types_builder>();
            pkg.register_package_extention<test::package::package_types_default_objects_builder>();
            pkg.complete_load();
        }

        m_olc.set_proto_local_set(&m_classes_local);
        m_olc.set_instance_local_set(&m_classes_local);
        m_olc.set_ownable_cache(&m_objects_cache);
        m_olc.set_objects_mapping(m_object_maping);
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

    void
    check_intance(root::smart_object& so)
    {
        ASSERT_TRUE(so.get_flags().instance_obj) << so.get_id();
        ASSERT_FALSE(so.get_flags().proto_obj) << so.get_id();
    }

    void
    check_proto(root::smart_object& so)
    {
        ASSERT_TRUE(so.get_flags().proto_obj);
        ASSERT_FALSE(so.get_flags().instance_obj);
    }

    core::line_cache<root::smart_object_ptr> m_objects_cache;

    core::cache_set m_classes_local;
    core::cache_set m_instances_local;
    core::object_load_context m_olc;
    std::vector<root::smart_object*> m_loaded_objects;
    std::shared_ptr<core::object_mapping> m_object_maping;
};

TEST_F(test_preloaded_test_package, load_class_object_with_custom_layout)
{
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    m_olc.set_prefix_path(obj_path);

    m_object_maping->add(AID("test_mesh"), false, APATH("game_objects/test_mesh.aobj"))
        .add(AID("test_material"), false, APATH("game_objects/test_material.aobj"));

    root::smart_object* obj = nullptr;
    auto rc = core::object_constructor::object_load(
        obj_path / "game_objects/test_complex_mesh_object.aobj", core::object_load_type::class_obj,
        m_olc, obj, m_loaded_objects);

    ASSERT_EQ(rc, result_code::ok);
    auto go = obj->as<root::game_object>();
    ASSERT_TRUE(go);
    check_proto(*go);

    ASSERT_EQ(m_loaded_objects.size(), 3);

    ASSERT_EQ(go->get_id(), AID("test_complex_mesh_object"));
    ASSERT_EQ(go->get_architype_id(), core::architype::game_object);

    auto components = go->get_subcomponents();
    ASSERT_EQ(components.size(), 2);

    auto comp1 = components[0];
    ASSERT_EQ(comp1->get_id(), AID("test_root_component_0"));
    ASSERT_EQ(comp1->get_architype_id(), core::architype::component);
    ASSERT_EQ(comp1->get_class_obj()->get_id(), AID("game_object_component"));
    check_proto(*comp1);

    auto comp2 = components[1]->as<base::mesh_component>();
    ASSERT_EQ(comp2->get_id(), AID("test_root_component_1"));
    ASSERT_EQ(comp2->get_architype_id(), core::architype::component);
    ASSERT_EQ(comp2->get_class_obj()->get_id(), AID("mesh_component"));
    check_proto(*comp2);

    auto mesh = comp2->get_mesh();
    ASSERT_EQ(mesh->get_id(), AID("test_mesh"));
    ASSERT_EQ(mesh->get_class_obj()->get_id(), AID("mesh"));
    check_proto(*mesh);

    auto material = comp2->get_material()->as<base::simple_texture_material>();
    ASSERT_EQ(material->get_id(), AID("test_material"));
    ASSERT_EQ(material->get_class_obj()->get_id(), AID("simple_texture_material"));
    check_proto(*material);

    auto& ts = material->get_sample(AID("simple_texture"));

    ASSERT_EQ(ts.sampler_id, AID("default"));
    ASSERT_EQ(ts.slot, 0);
    check_proto(*ts.txt);

    ASSERT_EQ(ts.txt->get_id(), AID("texture"));
}

TEST_F(test_preloaded_test_package, load_instance_object_with_custom_layout)
{
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    m_olc.set_prefix_path(obj_path);

    m_object_maping->add(AID("test_mesh"), false, APATH("game_objects/test_mesh.aobj"))
        .add(AID("test_material"), false, APATH("game_objects/test_material.aobj"));

    root::smart_object* obj = nullptr;
    auto rc = core::object_constructor::object_load(
        obj_path / "game_objects/test_complex_mesh_object.aobj",
        core::object_load_type::instance_obj, m_olc, obj, m_loaded_objects);

    ASSERT_EQ(rc, result_code::ok);
    auto go = obj->as<root::game_object>();
    ASSERT_TRUE(go);
    check_intance(*go);

    ASSERT_EQ(go->get_id(), AID("test_complex_mesh_object"));
    ASSERT_EQ(go->get_architype_id(), core::architype::game_object);

    auto components = go->get_subcomponents();
    ASSERT_EQ(components.size(), 2);

    auto comp1 = components[0];
    ASSERT_EQ(comp1->get_id(), AID("test_root_component_0"));
    ASSERT_EQ(comp1->get_architype_id(), core::architype::component);
    ASSERT_EQ(comp1->get_class_obj()->get_id(), AID("game_object_component"));
    check_intance(*comp1);

    auto comp2 = components[1]->as<base::mesh_component>();
    ASSERT_EQ(comp2->get_id(), AID("test_root_component_1"));
    ASSERT_EQ(comp2->get_architype_id(), core::architype::component);
    ASSERT_EQ(comp2->get_class_obj()->get_id(), AID("mesh_component"));
    check_intance(*comp2);

    auto mesh = comp2->get_mesh();
    ASSERT_EQ(mesh->get_id(), AID("test_mesh"));
    ASSERT_EQ(mesh->get_class_obj()->get_id(), AID("test_mesh"));
    check_intance(*mesh);

    auto material = comp2->get_material()->as<base::simple_texture_material>();
    ASSERT_EQ(material->get_id(), AID("test_material"));
    ASSERT_EQ(material->get_class_obj()->get_id(), AID("test_material"));
    check_intance(*material);

    auto& ts = material->get_sample(AID("simple_texture"));

    ASSERT_EQ(ts.sampler_id, AID("default"));
    ASSERT_EQ(ts.slot, 0);
    check_intance(*ts.txt);

    ASSERT_EQ(ts.txt->get_id(), AID("texture"));
}

TEST_F(test_preloaded_test_package, check_load_in_construct)
{
    auto& gs = glob::glob_state();

    auto proto_obj = test::package::instance().get_proto_local_cs().components.get_item(
        AID("test_complex_mesh_component"));

    ASSERT_EQ(proto_obj->get_id(), AID("test_complex_mesh_component"));
    ASSERT_EQ(proto_obj->get_architype_id(), core::architype::component);

    core::level l(AID("test_level"));

    test::test_mesh_object::construct_params sp;
    auto obj = l.spawn_object<test::test_mesh_object>(AID("aaa"), sp);

    ASSERT_TRUE(obj);
    ASSERT_EQ(obj->get_id(), AID("aaa"));
    check_intance(*obj);
}

TEST_F(test_preloaded_test_package, object_clone_creates_copy_with_new_id)
{
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    m_olc.set_prefix_path(obj_path);

    m_object_maping->add(AID("test_mesh"), false, APATH("game_objects/test_mesh.aobj"))
        .add(AID("test_material"), false, APATH("game_objects/test_material.aobj"));

    // First load source object as class/proto
    root::smart_object* src_obj = nullptr;
    auto rc = core::object_constructor::object_load(
        obj_path / "game_objects/test_complex_mesh_object.aobj", core::object_load_type::class_obj,
        m_olc, src_obj, m_loaded_objects);
    ASSERT_EQ(rc, result_code::ok);
    ASSERT_TRUE(src_obj);

    // Clone it with a new ID
    root::smart_object* cloned_obj = nullptr;
    std::vector<root::smart_object*> cloned_loaded;
    rc = core::object_constructor::object_clone(*src_obj, core::object_load_type::instance_obj,
                                                AID("cloned_mesh_object"), m_olc, cloned_obj,
                                                cloned_loaded);

    ASSERT_EQ(rc, result_code::ok);
    ASSERT_TRUE(cloned_obj);
    ASSERT_NE(src_obj, cloned_obj);

    // Verify cloned object has new ID but same type
    ASSERT_EQ(cloned_obj->get_id(), AID("cloned_mesh_object"));
    ASSERT_EQ(cloned_obj->get_type_id(), src_obj->get_type_id());
    check_intance(*cloned_obj);

    // Verify cloned object references original as class_obj
    ASSERT_EQ(cloned_obj->get_class_obj(), src_obj);

    // Verify game object structure was cloned
    auto src_go = src_obj->as<root::game_object>();
    auto cloned_go = cloned_obj->as<root::game_object>();
    ASSERT_TRUE(cloned_go);
    ASSERT_EQ(cloned_go->get_subcomponents().size(), src_go->get_subcomponents().size());
}

TEST_F(test_preloaded_test_package, object_instantiate_creates_instance_from_proto)
{
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) / "test.alvl";
    m_olc.set_prefix_path(obj_path);

    m_object_maping->add(AID("test_mesh"), false, APATH("game_objects/test_mesh.aobj"))
        .add(AID("test_material"), false, APATH("game_objects/test_material.aobj"));

    // Load proto object
    root::smart_object* proto_obj = nullptr;
    auto rc = core::object_constructor::object_load(
        obj_path / "game_objects/test_complex_mesh_object.aobj", core::object_load_type::class_obj,
        m_olc, proto_obj, m_loaded_objects);
    ASSERT_EQ(rc, result_code::ok);
    check_proto(*proto_obj);

    // Instantiate from proto
    root::smart_object* instance_obj = nullptr;
    std::vector<root::smart_object*> instance_loaded;
    rc = core::object_constructor::object_instantiate(*proto_obj, AID("instance_mesh_object"),
                                                      m_olc, instance_obj, instance_loaded);

    ASSERT_EQ(rc, result_code::ok);
    ASSERT_TRUE(instance_obj);
    ASSERT_NE(proto_obj, instance_obj);

    // Verify instance properties
    ASSERT_EQ(instance_obj->get_id(), AID("instance_mesh_object"));
    ASSERT_EQ(instance_obj->get_type_id(), proto_obj->get_type_id());
    check_intance(*instance_obj);

    // Instance should reference proto as class_obj
    ASSERT_EQ(instance_obj->get_class_obj(), proto_obj);

    // Verify components were instantiated
    auto proto_go = proto_obj->as<root::game_object>();
    auto instance_go = instance_obj->as<root::game_object>();
    ASSERT_TRUE(instance_go);
    ASSERT_EQ(instance_go->get_subcomponents().size(), proto_go->get_subcomponents().size());

    // Verify each component is an instance, not the same object
    for (size_t i = 0; i < proto_go->get_subcomponents().size(); ++i)
    {
        auto proto_comp = proto_go->get_subcomponents()[i];
        auto instance_comp = instance_go->get_subcomponents()[i];
        ASSERT_NE(proto_comp, instance_comp);
        check_intance(*instance_comp);
    }
}