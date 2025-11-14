
#include "core/object_constructor.h"

#include <core/package_manager.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <core/global_state.h>
#include <core/reflection/reflection_type.h>
#include <core/architype.h>
#include <core/object_load_context.h>
#include <testing/testing.h>

#include "packages/root/package.root.h"

#include <packages/root/model/game_object.h>

#include <utils/agea_log.h>
#include <utils/file_utils.h>

#include <gtest/gtest.h>

using namespace agea;

namespace
{

void
validate_empty_cache(core::state& gs)
{
    for (auto i = core::architype::first; i < core::architype::last;
         i = (core::architype)((uint8_t)i + 1))
    {
        ASSERT_TRUE(gs.getr_class_cache_map().get_cache(i)->get_items().empty())
            << "Failed at " << ::agea::core::to_string(i);
    }
}

root::smart_object_state_flag
as_flag(core::object_load_type t)
{
    return t == core::object_load_type::class_obj ? root::smart_object_state_flag::proto_obj
                                                  : root::smart_object_state_flag::instance_obj;
}

}  // namespace

struct test_root_package : public base_test
{
    void
    SetUp()
    {
        base_test::SetUp();
        root::package::reset_instance();
        glob::glob_state_reset();
    }

    void
    TearDown()
    {
        root::package::reset_instance();
        glob::glob_state_reset();

        base_test::TearDown();
    }
};

TEST_F(test_root_package, basic_load)
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

    validate_empty_cache(gs);

    pkg.complete_load();

    ASSERT_EQ(pkg.get_reflection_types().size(), 26);

    auto& lc = pkg.get_proto_local_cs();

    ASSERT_EQ(lc.objects.get_size(), 10);
    ASSERT_EQ(lc.components.get_size(), 3);
    ASSERT_EQ(lc.game_objects.get_size(), 1);
    ASSERT_EQ(lc.materials.get_size(), 1);
    ASSERT_EQ(lc.meshes.get_size(), 1);
    ASSERT_EQ(lc.textures.get_size(), 1);
    ASSERT_EQ(lc.shader_effects.get_size(), 1);

    pkg.complete_unload();

    ASSERT_TRUE(gs.getr_rm().get_types_to_id().empty());
    ASSERT_TRUE(gs.getr_rm().get_types_to_name().empty());
    validate_empty_cache(gs);
}

struct test_preloaded_root_package : base_test_with_params<core::object_load_type>
{
    void
    SetUp()
    {
        root::package::reset_instance();
        glob::glob_state_reset();

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
                               core::state_mutator__resource_locator::set(s);
                           });
        gs.run_create();

        root::package::init_instance();
        auto& pkg = root::package::instance();
        pkg.register_package_extention<root::package::package_types_builder>();
        pkg.register_package_extention<root::package::package_types_default_objects_builder>();

        validate_empty_cache(gs);

        pkg.complete_load();
    }

    void
    TearDown()
    {
        root::package::instance().complete_unload();
        root::package::reset_instance();
        glob::glob_state_reset();

        base_test_with_params::TearDown();
    }

    void
    set_up_caches(const agea::utils::path& p)
    {
        auto load_type = GetParam();
        m_olc.set_prefix_path(p).set_ownable_cache(&m_objects_cache);
        if (load_type == core::object_load_type::class_obj)
        {
            m_olc.set_proto_local_set(&m_cs);
        }
        else
        {
            m_olc.set_instance_local_set(&m_cs);
        }
    }

    core::line_cache<root::smart_object_ptr> m_objects_cache;

    core::cache_set m_cs;
    core::object_load_context m_olc;
};

TEST_P(test_preloaded_root_package, load_instance_object_with_custom_layout)
{
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) /
                    "test.alvl/game_objects/test_obj_custom_layout.aobj";

    set_up_caches(obj_path);

    root::smart_object* obj = nullptr;
    std::vector<root::smart_object*> loaded_objects;
    auto load_type = GetParam();
    auto rc =
        core::object_constructor::object_load_v2(obj_path, load_type, m_olc, obj, loaded_objects);

    ASSERT_EQ(rc, result_code::ok);
    ASSERT_TRUE(obj);
    ASSERT_TRUE(obj->has_flag(as_flag(load_type)));

    ASSERT_EQ(loaded_objects.size(), 3);
    auto go = obj->as<root::game_object>();

    ASSERT_EQ(go->get_id(), AID("test_obj_custom_layout"));
    ASSERT_EQ(go->get_architype_id(), core::architype::game_object);

    auto components = go->get_subcomponents();
    ASSERT_EQ(components.size(), 2);

    auto comp1 = components[0];
    ASSERT_EQ(comp1->get_id(), AID("test_component_1"));
    ASSERT_EQ(comp1->get_architype_id(), core::architype::component);
    ASSERT_EQ(comp1->get_class_obj()->get_id(), AID("game_object_component"));
    ASSERT_TRUE(comp1->has_flag(as_flag(load_type)));

    auto comp2 = components[1];
    ASSERT_EQ(comp2->get_id(), AID("test_component_2"));
    ASSERT_EQ(comp2->get_architype_id(), core::architype::component);
    ASSERT_EQ(comp2->get_class_obj()->get_id(), AID("game_object_component"));
    ASSERT_TRUE(comp2->has_flag(as_flag(load_type)));
}

TEST_P(test_preloaded_root_package, load_class_object_without_layout)
{
    auto& gs = glob::glob_state();
    auto obj_path = gs.get_resource_locator()->resource_dir(category::levels) /
                    "test.alvl/game_objects/test_obj.aobj";

    core::line_cache<root::smart_object_ptr> objects_cache;

    core::cache_set cs;

    core::object_load_context olc;
    olc.set_prefix_path(obj_path).set_ownable_cache(&objects_cache).set_instance_local_set(&cs);

    root::smart_object* obj = nullptr;
    std::vector<root::smart_object*> loaded_objects;

    core::object_load_type load_type = GetParam();

    auto rc =
        core::object_constructor::object_load_v2(obj_path, load_type, olc, obj, loaded_objects);

    ASSERT_EQ(rc, result_code::ok);
    ASSERT_TRUE(obj);
    ASSERT_TRUE(obj->has_flag(as_flag(load_type)));

    ASSERT_EQ(loaded_objects.size(), 2);
    auto go = obj->as<root::game_object>();

    ASSERT_EQ(go->get_id(), AID("test_obj"));
    ASSERT_EQ(go->get_architype_id(), core::architype::game_object);

    auto components = go->get_subcomponents();
    ASSERT_EQ(components.size(), 1);

    auto comp1 = components[0];
    ASSERT_EQ(comp1->get_id(), AID("root_component#2")) << comp1->get_id();
    ASSERT_EQ(comp1->get_architype_id(), core::architype::component);
    ASSERT_EQ(comp1->get_class_obj()->get_id(), AID("root_component#1"))
        << comp1->get_class_obj()->get_id();
    ASSERT_TRUE(comp1->has_flag(as_flag(load_type)));
}

INSTANTIATE_TEST_SUITE_P(test_object_load,
                         test_preloaded_root_package,
                         ::testing::Values(core::object_load_type::instance_obj,
                                           core::object_load_type::class_obj));