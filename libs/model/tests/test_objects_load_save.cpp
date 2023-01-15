#include "base_test.h"

#include <model/caches/game_objects_cache.h>
#include <model/caches/empty_objects_cache.h>
#include <model/caches/caches_map.h>

#include <model/object_constructor.h>
#include <model/object_construction_context.h>
#include <model/level.h>
#include <model/level_constructor.h>

#include <model/game_object.h>
#include <model/objects_mapping.h>
#include <model/components/mesh_component.h>
#include <model/assets/material.h>
#include <model/assets/mesh.h>

#include <serialization/serialization.h>

#include <utils/file_utils.h>
#include <utils/agea_log.h>

#include <gtest/gtest.h>

using namespace agea;

struct test_object_constructor : base_test
{
    void
    SetUp()
    {
        base_test::SetUp();

        {
            auto prefix = glob::resource_locator::get()->resource(category::packages, "test.apkg");

            model::object_mapping om;

            om.buiild_object_mapping(prefix / APATH("package.acfg"));

            occ.set_prefix_path(prefix)
                .set_class_global_set(&global_class_objects_cs)
                .set_class_local_set(&local_class_objects_cs)
                .set_instance_global_set(&global_objects_cs)
                .set_instance_local_set(&local_objects_cs)
                .set_ownable_cache(&objs)
                .set_objects_mapping(om.m_items);
        }
    }

    void
    check_item_in_caches(utils::id id, model::architype atype, bool check_in_class)
    {
        for (auto p : global_class_objects_cs.map->get_items())
        {
            ASSERT_FALSE(p.second->has_item(id));
        }

        for (auto p : global_objects_cs.map->get_items())
        {
            ASSERT_FALSE(p.second->has_item(id));
        }

        auto to_exists_class = check_in_class ? &local_class_objects_cs : &local_objects_cs;
        auto to_not_exists_class = !check_in_class ? &local_class_objects_cs : &local_objects_cs;

        for (auto& p : to_exists_class->map->get_items())
        {
            if (p.first != atype && p.first != model::architype::smart_object)
            {
                ASSERT_FALSE(p.second->has_item(id)) << (int)p.first;
            }
            else
            {
                ASSERT_TRUE(p.second->has_item(id));
            }
        }

        for (auto& p : to_not_exists_class->map->get_items())
        {
            ASSERT_FALSE(p.second->has_item(id));
        }
    }

    model::cache_set local_class_objects_cs;
    model::cache_set global_class_objects_cs;
    model::cache_set local_objects_cs;
    model::cache_set global_objects_cs;
    model::line_cache<model::smart_object_ptr> objs;

    model::object_constructor_context occ;
};

bool
is_from_EO_cache(model::smart_object* obj)
{
    return glob::empty_objects_cache::get()->get(obj->get_type_id()) == obj;
}

TEST_F(test_object_constructor, load_and_save_class_component)
{
    auto mt_red = model::object_constructor::create_empty_object<model::material>(AID("mt_red"));
    auto cube_mesh = model::object_constructor::create_empty_object<model::mesh>(AID("cube_mesh"));

    occ.get_class_global_set()->map->add_item(*mt_red);
    occ.get_class_global_set()->map->add_item(*cube_mesh);

    occ.set_construction_type(model::object_constructor_context::construction_type::class_obj);

    model::smart_object* obj = nullptr;
    auto rc = model::object_constructor::object_load(
        APATH("class/components/cube_mesh_component.aobj"), occ, obj);
    ASSERT_TRUE(!!obj);
    ASSERT_EQ(rc, result_code::ok);

    auto component = obj->as<model::mesh_component>();

    ASSERT_EQ(component->get_id(), AID("cube_mesh_component"));
    ASSERT_EQ(component->get_type_id(), AID("mesh_component"));
    ASSERT_EQ(component->get_order_idx(), model::NO_index);
    ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
    ASSERT_EQ(component->get_material(), mt_red.get());
    ASSERT_EQ(component->get_mesh(), cube_mesh.get());

    ASSERT_FALSE(is_from_EO_cache(component));

    check_item_in_caches(AID("cube_mesh_component"), model::architype::component, true);

    ASSERT_EQ(objs.get_size(), 1U);

    rc = model::object_constructor::object_save(*obj, get_current_workspace() / "save.aobj");
    ASSERT_EQ(rc, result_code::ok);

    utils::path p;
    occ.make_full_path(APATH("class/components/cube_mesh_component.aobj"), p);

    auto result = utils::file_utils::compare_files(p, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, load_and_save_class_component__subobjects_are_instances)
{
    auto mt_red = model::object_constructor::create_empty_object<model::material>(AID("mt_red"));
    occ.get_instance_global_set()->map->add_item(*mt_red);
    mt_red->set_state(agea::model::smart_object_internal_state::instance_obj);

    auto cube_mesh = model::object_constructor::create_empty_object<model::mesh>(AID("cube_mesh"));
    occ.get_instance_global_set()->map->add_item(*cube_mesh);
    cube_mesh->set_state(agea::model::smart_object_internal_state::instance_obj);

    occ.set_construction_type(model::object_constructor_context::construction_type::class_obj);

    model::smart_object* obj = nullptr;
    auto rc = model::object_constructor::object_load(
        APATH("class/components/cube_mesh_component.aobj"), occ, obj);

    ASSERT_EQ(obj, nullptr);
    ASSERT_EQ(rc, result_code::path_not_found);
}

TEST_F(test_object_constructor, load_and_save_instance_component)
{
    auto mt_red = model::object_constructor::create_empty_object<model::material>(AID("mt_red"));
    auto cube_mesh = model::object_constructor::create_empty_object<model::mesh>(AID("cube_mesh"));

    occ.get_instance_global_set()->map->add_item(*mt_red);
    occ.get_instance_global_set()->map->add_item(*cube_mesh);
    occ.set_construction_type(model::object_constructor_context::construction_type::instance_obj);
    model::smart_object* obj = nullptr;
    model::object_constructor::object_load(APATH("class/components/cube_mesh_component.aobj"), occ,
                                           obj);
    ASSERT_TRUE(!!obj);

    auto component = obj->as<model::mesh_component>();

    ASSERT_EQ(component->get_id(), AID("cube_mesh_component"));
    ASSERT_EQ(component->get_type_id(), AID("mesh_component"));
    ASSERT_EQ(component->get_order_idx(), model::NO_index);
    ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
    ASSERT_EQ(component->get_material(), mt_red.get());
    ASSERT_EQ(component->get_mesh(), cube_mesh.get());

    ASSERT_FALSE(is_from_EO_cache(component));

    check_item_in_caches(AID("cube_mesh_component"), model::architype::component, false);

    ASSERT_EQ(objs.get_size(), 1U);

    auto rc = model::object_constructor::object_save(*obj, get_current_workspace() / "save.aobj");
    ASSERT_EQ(rc, result_code::ok);

    utils::path p;
    occ.make_full_path(APATH("class/components/cube_mesh_component.aobj"), p);

    auto result = utils::file_utils::compare_files(p, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, load_and_save_instance_component__subobjects_are_classes)
{
    auto mt_red = model::object_constructor::create_empty_object<model::material>(AID("mt_red"));
    auto cube_mesh = model::object_constructor::create_empty_object<model::mesh>(AID("cube_mesh"));

    occ.get_class_global_set()->map->add_item(*mt_red);
    occ.get_class_global_set()->map->add_item(*cube_mesh);
    occ.set_construction_type(model::object_constructor_context::construction_type::instance_obj);
    model::smart_object* obj = nullptr;
    model::object_constructor::object_load(APATH("class/components/cube_mesh_component.aobj"), occ,
                                           obj);
    ASSERT_EQ(obj, nullptr);
}

TEST_F(test_object_constructor, load_and_save_derived_class_component)
{
    auto mt_red = model::object_constructor::create_empty_object<model::material>(AID("mt_red"));
    auto mt_green =
        model::object_constructor::create_empty_object<model::material>(AID("mt_green"));
    auto cube_mesh = model::object_constructor::create_empty_object<model::mesh>(AID("cube_mesh"));

    result_code rc = result_code::nav;

    occ.get_class_global_set()->map->add_item(*mt_red);
    occ.get_class_global_set()->map->add_item(*mt_green);
    occ.get_class_global_set()->map->add_item(*cube_mesh);
    occ.set_construction_type(model::object_constructor_context::construction_type::class_obj);

    model::smart_object* obj = nullptr;
    {
        rc = model::object_constructor::object_load(
            APATH("class/components/cube_mesh_component.aobj"), occ, obj);
        ASSERT_TRUE(!!obj);
        ASSERT_EQ(rc, result_code::ok);
    }

    rc = model::object_constructor::object_load(
        APATH("class/components/cube_mesh_component_derived.aobj"), occ, obj);
    ASSERT_TRUE(!!obj);
    ASSERT_EQ(rc, result_code::ok);

    auto component = obj->as<model::mesh_component>();

    ASSERT_EQ(component->get_id(), AID("cube_mesh_component_derived"));
    ASSERT_EQ(component->get_type_id(), AID("mesh_component"));
    ASSERT_EQ(component->get_order_idx(), model::NO_index);
    ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
    ASSERT_EQ(component->get_material(), mt_green.get());
    ASSERT_EQ(component->get_mesh(), cube_mesh.get());

    ASSERT_FALSE(is_from_EO_cache(component));

    check_item_in_caches(AID("cube_mesh_component_derived"), model::architype::component, true);

    ASSERT_EQ(objs.get_size(), 2U);

    rc = model::object_constructor::object_save(*obj, get_current_workspace() / "save.aobj");
    ASSERT_EQ(rc, result_code::ok);

    utils::path p;
    occ.make_full_path(APATH("class/components/cube_mesh_component_derived.aobj"), p);

    auto result = utils::file_utils::compare_files(p, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, load_and_save_derived_class_component__without_preload)
{
    auto mt_red = model::object_constructor::create_empty_object<model::material>(AID("mt_red"));
    auto mt_green =
        model::object_constructor::create_empty_object<model::material>(AID("mt_green"));
    auto cube_mesh = model::object_constructor::create_empty_object<model::mesh>(AID("cube_mesh"));

    occ.get_class_global_set()->map->add_item(*mt_red);
    occ.get_class_global_set()->map->add_item(*mt_green);
    occ.get_class_global_set()->map->add_item(*cube_mesh);

    occ.set_construction_type(model::object_constructor_context::construction_type::class_obj);
    model::smart_object* obj = nullptr;
    model::object_constructor::object_load(
        APATH("class/components/cube_mesh_component_derived.aobj"), occ, obj);
    ASSERT_TRUE(!!obj);

    auto component = obj->as<model::mesh_component>();

    ASSERT_EQ(component->get_id(), AID("cube_mesh_component_derived"));
    ASSERT_EQ(component->get_type_id(), AID("mesh_component"));
    ASSERT_EQ(component->get_order_idx(), model::NO_index);
    ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
    ASSERT_EQ(component->get_material(), mt_green.get());
    ASSERT_EQ(component->get_mesh(), cube_mesh.get());

    ASSERT_FALSE(is_from_EO_cache(component));

    check_item_in_caches(AID("cube_mesh_component_derived"), model::architype::component, true);

    ASSERT_EQ(objs.get_size(), 2U);

    auto rc = model::object_constructor::object_save(*obj, get_current_workspace() / "save.aobj");
    ASSERT_EQ(rc, result_code::ok);

    utils::path p;
    occ.make_full_path(APATH("class/components/cube_mesh_component_derived.aobj"), p);

    auto result = utils::file_utils::compare_files(p, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

/* ====================================================================================== */
TEST_F(test_object_constructor, load_and_save_class_object)
{
    auto mt_red = model::object_constructor::create_empty_object<model::material>(AID("mt_red"));
    auto cube_mesh = model::object_constructor::create_empty_object<model::mesh>(AID("cube_mesh"));
    auto mesh_component = model::object_constructor::create_empty_object<model::mesh_component>(
        AID("cube_mesh_component"));
    auto root_component =
        model::object_constructor::create_empty_object<model::game_object_component>(
            AID("root_component"));

    occ.get_class_global_set()->map->add_item(*mt_red);
    occ.get_class_global_set()->map->add_item(*cube_mesh);
    occ.get_class_global_set()->map->add_item(*mesh_component);
    occ.get_class_global_set()->map->add_item(*root_component);
    occ.set_construction_type(model::object_constructor_context::construction_type::class_obj);
    model::smart_object* obj = nullptr;
    auto rc = model::object_constructor::object_load(APATH("class/game_objects/cubes_chain.aobj"),
                                                     occ, obj);
    ASSERT_TRUE(!!obj);
    ASSERT_EQ(rc, result_code::ok);

    check_item_in_caches(AID("cubes_chain"), model::architype::game_object, true);

    auto game_object = obj->as<model::game_object>();
    ASSERT_TRUE(!!game_object);

    ASSERT_EQ(occ.get_class_local_set()->objects->get_size(), 4);
    ASSERT_EQ(occ.get_instance_local_set()->objects->get_size(), 0);

    ASSERT_EQ(game_object->get_id(), AID("cubes_chain"));
    ASSERT_EQ(game_object->get_type_id(), AID("mesh_object"));

    {
        auto component = game_object->get_component_at(0U);
        ASSERT_EQ(component->get_id(), AID("cube_chain_root_component"));
        ASSERT_EQ(component->get_type_id(), AID("game_object_component"));
        ASSERT_EQ(component->get_order_idx(), 0);
        ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->get_component_at(1U);
        ASSERT_EQ(component->get_id(), AID("cubes_chain_cube_mesh_1"));
        ASSERT_EQ(component->get_type_id(), AID("mesh_component"));
        ASSERT_EQ(component->get_order_idx(), 1);
        ASSERT_EQ(component->get_parent_idx(), 0);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->get_component_at(2U);
        ASSERT_EQ(component->get_id(), AID("cubes_chain_cube_mesh_2"));
        ASSERT_EQ(component->get_type_id(), AID("mesh_component"));
        ASSERT_EQ(component->get_order_idx(), 2);
        ASSERT_EQ(component->get_parent_idx(), 0);
        ASSERT_FALSE(is_from_EO_cache(component));
    }

    rc =
        model::object_constructor::object_save(*game_object, get_current_workspace() / "save.aobj");
    ASSERT_EQ(rc, result_code::ok);

    utils::path p;
    occ.make_full_path(APATH("class/game_objects/cubes_chain.aobj"), p);

    auto result = utils::file_utils::compare_files(p, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, load_and_save_instance_object)
{
    auto mt_red = model::object_constructor::create_empty_object<model::material>(AID("mt_red"));
    auto cube_mesh = model::object_constructor::create_empty_object<model::mesh>(AID("cube_mesh"));
    auto mesh_component = model::object_constructor::create_empty_object<model::mesh_component>(
        AID("cube_mesh_component"));
    auto root_component =
        model::object_constructor::create_empty_object<model::game_object_component>(
            AID("root_component"));

    occ.get_instance_global_set()->map->add_item(*mt_red);
    occ.get_instance_global_set()->map->add_item(*cube_mesh);
    occ.get_class_global_set()->map->add_item(*mesh_component);
    occ.get_class_global_set()->map->add_item(*root_component);
    occ.set_construction_type(model::object_constructor_context::construction_type::instance_obj);

    model::smart_object* obj = nullptr;
    auto rc = model::object_constructor::object_load(APATH("class/game_objects/cubes_chain.aobj"),
                                                     occ, obj);
    ASSERT_TRUE(!!obj);
    ASSERT_EQ(rc, result_code::ok);
    check_item_in_caches(AID("cubes_chain"), model::architype::game_object, false);

    auto game_object = obj->as<model::game_object>();
    ASSERT_TRUE(!!game_object);

    ASSERT_EQ(occ.get_instance_local_set()->objects->get_size(), 4);

    ASSERT_EQ(game_object->get_id(), AID("cubes_chain"));
    ASSERT_EQ(game_object->get_type_id(), AID("mesh_object"));

    {
        auto component = game_object->get_component_at(0U);
        ASSERT_EQ(component->get_id(), AID("cube_chain_root_component"));
        ASSERT_EQ(component->get_type_id(), AID("game_object_component"));
        ASSERT_EQ(component->get_order_idx(), 0);
        ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->get_component_at(1U);
        ASSERT_EQ(component->get_id(), AID("cubes_chain_cube_mesh_1"));
        ASSERT_EQ(component->get_type_id(), AID("mesh_component"));
        ASSERT_EQ(component->get_order_idx(), 1);
        ASSERT_EQ(component->get_parent_idx(), 0);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->get_component_at(2U);
        ASSERT_EQ(component->get_id(), AID("cubes_chain_cube_mesh_2"));
        ASSERT_EQ(component->get_type_id(), AID("mesh_component"));
        ASSERT_EQ(component->get_order_idx(), 2);
        ASSERT_EQ(component->get_parent_idx(), 0);
        ASSERT_FALSE(is_from_EO_cache(component));
    }

    rc =
        model::object_constructor::object_save(*game_object, get_current_workspace() / "save.aobj");
    ASSERT_EQ(rc, result_code::ok);

    utils::path p;
    occ.make_full_path(APATH("class/game_objects/cubes_chain.aobj"), p);

    auto result = utils::file_utils::compare_files(p, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}