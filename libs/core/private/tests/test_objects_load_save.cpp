#include "base_test.h"

#include <core/caches/game_objects_cache.h>
#include <core/caches/caches_map.h>

#include <core/object_constructor.h>
#include <core/object_load_context.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <core/package.h>
#include <root/game_object.h>
#include <core/objects_mapping.h>
#include <root/components/mesh_component.h>
#include <root/assets/material.h>
#include <root/assets/mesh.h>
#include <root/assets/texture.h>
#include <root/assets/shader_effect.h>

#include <serialization/serialization.h>

#include <utils/file_utils.h>
#include <utils/agea_log.h>
#include <list>

#include <gtest/gtest.h>

using namespace agea;

#if 0
namespace
{
std::vector<core::smart_object*> dummy_loaded_obj;
}

struct test_object_constructor : base_test
{
    void
    SetUp()
    {
        base_test::SetUp();

        {
            auto prefix = glob::resource_locator::get()->resource(category::packages, "test.apkg");

            std::shared_ptr<core::object_mapping> om = std::make_shared<core::object_mapping>();

            om->buiild_object_mapping(prefix / APATH("package.acfg"));

            occ.set_prefix_path(prefix)
                .set_proto_global_set(&global_class_objects_cs)
                .set_proto_local_set(&local_class_objects_cs)
                .set_instance_global_set(&global_objects_cs)
                .set_instance_local_set(&local_objects_cs)
                .set_ownable_cache(&objs)
                .set_objects_mapping(om);

            glob::level::create_ref(&level);
        }
    }

    void
    check_item_in_caches(const utils::id& id,
                         core::architype atype,
                         bool check_in_class,
                         bool check_in_instance)
    {
        // Should not exist in global caches
        for (auto p : global_class_objects_cs.map->get_items())
        {
            ASSERT_FALSE(p.second->has_item(id));
        }

        for (auto p : global_objects_cs.map->get_items())
        {
            ASSERT_FALSE(p.second->has_item(id));
        }

        if (check_in_class)
        {
            for (auto& p : local_class_objects_cs.map->get_items())
            {
                if (p.first != atype && p.first != core::architype::smart_object)
                {
                    ASSERT_FALSE(p.second->has_item(id)) << (int)p.first;
                }
                else
                {
                    ASSERT_TRUE(p.second->has_item(id));
                }
            }
        }
        else
        {
            for (auto& p : local_class_objects_cs.map->get_items())
            {
                ASSERT_FALSE(p.second->has_item(id));
            }
        }

        if (check_in_instance)
        {
            for (auto& p : local_objects_cs.map->get_items())
            {
                if (p.first != atype && p.first != core::architype::smart_object)
                {
                    ASSERT_FALSE(p.second->has_item(id)) << (int)p.first;
                }
                else
                {
                    ASSERT_TRUE(p.second->has_item(id));
                }
            }
        }
        else
        {
            for (auto& p : local_objects_cs.map->get_items())
            {
                ASSERT_FALSE(p.second->has_item(id));
            }
        }
    }

    core::cache_set local_class_objects_cs;
    core::cache_set global_class_objects_cs;
    core::cache_set local_objects_cs;
    core::cache_set global_objects_cs;
    core::line_cache<root::smart_object_ptr> objs;

    core::object_load_context occ;
    core::level level;
    singleton_registry reg;
};

bool
is_from_EO_cache(root::smart_object* obj)
{
    true;  // return glob::empty_objects_cache::get()->get_item(obj->get_type_id()) == obj;
}

TEST_F(test_object_constructor, load_and_save_class_component)
{
    auto mt_red = core::object_constructor::alloc_empty_object<root::material>(AID("mt_red"));
    auto cube_mesh = core::object_constructor::alloc_empty_object<root::mesh>(AID("cube_mesh"));

    occ.get_proto_global_set()->map->add_item(*mt_red);
    occ.get_proto_global_set()->map->add_item(*cube_mesh);

    root::smart_object* obj = nullptr;
    auto rc = core::object_constructor::object_load(
        APATH("class/components/cube_mesh_component.aobj"), core::object_load_type::class_obj, occ,
        obj, dummy_loaded_obj);
    ASSERT_TRUE(!!obj);
    ASSERT_EQ(rc, result_code::ok);

    auto component = obj->as<root::mesh_component>();

    ASSERT_EQ(component->get_id(), AID("cube_mesh_component"));
    ASSERT_EQ(component->get_type_id(), AID("mesh_component"));
    ASSERT_EQ(component->get_order_idx(), core::NO_index);
    ASSERT_EQ(component->get_parent_idx(), core::NO_parent);
    ASSERT_EQ(component->get_material(), mt_red.get());
    ASSERT_EQ(component->get_mesh(), cube_mesh.get());

    ASSERT_FALSE(is_from_EO_cache(component));

    check_item_in_caches(AID("cube_mesh_component"), core::architype::component, true, false);

    ASSERT_EQ(objs.get_size(), 1U);

    rc = core::object_constructor::object_save(*obj, get_current_workspace() / "save.aobj");
    ASSERT_EQ(rc, result_code::ok);

    utils::path p;
    occ.make_full_path(APATH("class/components/cube_mesh_component.aobj"), p);

    auto result = utils::file_utils::compare_files(p, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, load_and_save_class_component__subobjects_are_instances)
{
    auto mt_red = core::object_constructor::alloc_empty_object<core::material>(AID("mt_red"));
    occ.get_instance_global_set()->map->add_item(*mt_red);
    mt_red->set_flag(agea::core::smart_object_state_flag::instance_obj);

    auto cube_mesh = core::object_constructor::alloc_empty_object<core::mesh>(AID("cube_mesh"));
    occ.get_instance_global_set()->map->add_item(*cube_mesh);
    cube_mesh->set_flag(agea::core::smart_object_state_flag::instance_obj);

    core::smart_object* obj = nullptr;
    auto rc = core::object_constructor::object_load(
        APATH("class/components/cube_mesh_component.aobj"), core::object_load_type::class_obj, occ,
        obj, dummy_loaded_obj);

    ASSERT_EQ(obj, nullptr);
    ASSERT_EQ(rc, result_code::path_not_found);
}

TEST_F(test_object_constructor, load_and_save_instance_component)
{
    auto mt_red = core::object_constructor::alloc_empty_object<core::material>(AID("mt_red"));
    auto cube_mesh = core::object_constructor::alloc_empty_object<core::mesh>(AID("cube_mesh"));

    occ.get_instance_global_set()->map->add_item(*mt_red);
    occ.get_instance_global_set()->map->add_item(*cube_mesh);
    core::smart_object* obj = nullptr;
    core::object_constructor::object_load(APATH("class/components/cube_mesh_component.aobj"),
                                          core::object_load_type::instance_obj, occ, obj,
                                          dummy_loaded_obj);
    ASSERT_TRUE(!!obj);

    auto component = obj->as<core::mesh_component>();

    ASSERT_EQ(component->get_id(), AID("cube_mesh_component"));
    ASSERT_EQ(component->get_type_id(), AID("mesh_component"));
    ASSERT_EQ(component->get_order_idx(), core::NO_index);
    ASSERT_EQ(component->get_parent_idx(), core::NO_parent);
    ASSERT_EQ(component->get_material(), mt_red.get());
    ASSERT_EQ(component->get_mesh(), cube_mesh.get());

    ASSERT_FALSE(is_from_EO_cache(component));

    check_item_in_caches(AID("cube_mesh_component"), core::architype::component, false, true);

    ASSERT_EQ(objs.get_size(), 1U);

    auto rc = core::object_constructor::object_save(*obj, get_current_workspace() / "save.aobj");
    ASSERT_EQ(rc, result_code::ok);

    utils::path p;
    occ.make_full_path(APATH("class/components/cube_mesh_component.aobj"), p);

    auto result = utils::file_utils::compare_files(p, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, load_and_save_instance_component__subobjects_are_classes)
{
    auto mt_red = core::object_constructor::alloc_empty_object<core::material>(AID("mt_red"));
    auto cube_mesh = core::object_constructor::alloc_empty_object<core::mesh>(AID("cube_mesh"));

    occ.get_proto_global_set()->map->add_item(*mt_red);
    occ.get_proto_global_set()->map->add_item(*cube_mesh);

    core::smart_object* obj = nullptr;
    core::object_constructor::object_load(APATH("class/components/cube_mesh_component.aobj"),
                                          core::object_load_type::instance_obj, occ, obj,
                                          dummy_loaded_obj);
    ASSERT_EQ(obj, nullptr);
}

TEST_F(test_object_constructor, load_and_save_derived_class_component)
{
    auto mt_red = core::object_constructor::alloc_empty_object<core::material>(AID("mt_red"));
    auto mt_green = core::object_constructor::alloc_empty_object<core::material>(AID("mt_green"));
    auto cube_mesh = core::object_constructor::alloc_empty_object<core::mesh>(AID("cube_mesh"));

    result_code rc = result_code::nav;

    occ.get_proto_global_set()->map->add_item(*mt_red);
    occ.get_proto_global_set()->map->add_item(*mt_green);
    occ.get_proto_global_set()->map->add_item(*cube_mesh);

    core::smart_object* obj = nullptr;
    {
        rc = core::object_constructor::object_load(
            APATH("class/components/cube_mesh_component.aobj"), core::object_load_type::class_obj,
            occ, obj, dummy_loaded_obj);
        ASSERT_TRUE(!!obj);
        ASSERT_EQ(rc, result_code::ok);
    }

    rc = core::object_constructor::object_load(
        APATH("class/components/cube_mesh_component_derived.aobj"),
        core::object_load_type::class_obj, occ, obj, dummy_loaded_obj);
    ASSERT_TRUE(!!obj);
    ASSERT_EQ(rc, result_code::ok);

    auto component = obj->as<core::mesh_component>();

    ASSERT_EQ(component->get_id(), AID("cube_mesh_component_derived"));
    ASSERT_EQ(component->get_type_id(), AID("mesh_component"));
    ASSERT_EQ(component->get_order_idx(), core::NO_index);
    ASSERT_EQ(component->get_parent_idx(), core::NO_parent);
    ASSERT_EQ(component->get_material(), mt_green.get());
    ASSERT_EQ(component->get_mesh(), cube_mesh.get());

    ASSERT_FALSE(is_from_EO_cache(component));

    check_item_in_caches(AID("cube_mesh_component_derived"), core::architype::component, true,
                         false);

    ASSERT_EQ(objs.get_size(), 2U);

    rc = core::object_constructor::object_save(*obj, get_current_workspace() / "save.aobj");
    ASSERT_EQ(rc, result_code::ok);

    utils::path p;
    occ.make_full_path(APATH("class/components/cube_mesh_component_derived.aobj"), p);

    auto result = utils::file_utils::compare_files(p, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, load_and_save_derived_class_component__without_preload)
{
    auto mt_red = core::object_constructor::alloc_empty_object<core::material>(AID("mt_red"));
    auto mt_green = core::object_constructor::alloc_empty_object<core::material>(AID("mt_green"));
    auto cube_mesh = core::object_constructor::alloc_empty_object<core::mesh>(AID("cube_mesh"));

    occ.get_proto_global_set()->map->add_item(*mt_red);
    occ.get_proto_global_set()->map->add_item(*mt_green);
    occ.get_proto_global_set()->map->add_item(*cube_mesh);

    core::smart_object* obj = nullptr;
    auto rc = core::object_constructor::object_load(
        APATH("class/components/cube_mesh_component_derived.aobj"),
        core::object_load_type::class_obj, occ, obj, dummy_loaded_obj);
    ASSERT_EQ(rc, result_code::proto_doesnt_exist);
    ASSERT_TRUE(!obj);
}

/* ====================================================================================== */
TEST_F(test_object_constructor, load_and_save_class_object)
{
    auto mt_red = core::object_constructor::alloc_empty_object<core::material>(AID("mt_red"));
    auto cube_mesh = core::object_constructor::alloc_empty_object<core::mesh>(AID("cube_mesh"));
    auto mesh_component = core::object_constructor::alloc_empty_object<core::mesh_component>(
        AID("cube_mesh_component"));
    auto root_component = core::object_constructor::alloc_empty_object<core::game_object_component>(
        AID("root_component"));

    occ.get_proto_global_set()->map->add_item(*mt_red);
    occ.get_proto_global_set()->map->add_item(*cube_mesh);
    occ.get_proto_global_set()->map->add_item(*mesh_component);
    occ.get_proto_global_set()->map->add_item(*root_component);
    core::smart_object* obj = nullptr;
    auto rc = core::object_constructor::object_load(APATH("class/game_objects/cubes_chain.aobj"),
                                                    core::object_load_type::class_obj, occ, obj,
                                                    dummy_loaded_obj);
    ASSERT_TRUE(!!obj);
    ASSERT_EQ(rc, result_code::ok);

    check_item_in_caches(AID("cubes_chain"), core::architype::game_object, true, false);

    auto game_object = obj->as<core::game_object>();
    ASSERT_TRUE(!!game_object);

    ASSERT_EQ(occ.get_proto_local_set()->objects->get_size(), 4);
    ASSERT_EQ(occ.get_instance_local_set()->objects->get_size(), 0);

    ASSERT_EQ(game_object->get_id(), AID("cubes_chain"));
    ASSERT_EQ(game_object->get_type_id(), AID("mesh_object"));

    {
        auto component = game_object->get_component_at(0U);
        ASSERT_EQ(component->get_id(), AID("cube_chain_root_component"));
        ASSERT_EQ(component->get_type_id(), AID("game_object_component"));
        ASSERT_EQ(component->get_order_idx(), 0);
        ASSERT_EQ(component->get_parent_idx(), core::NO_parent);
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

    rc = core::object_constructor::object_save(*game_object, get_current_workspace() / "save.aobj");
    ASSERT_EQ(rc, result_code::ok);

    utils::path p;
    occ.make_full_path(APATH("class/game_objects/cubes_chain.aobj"), p);

    auto result = utils::file_utils::compare_files(p, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, load_and_save_instance_object)
{
    auto se_simple_texture =
        core::object_constructor::alloc_empty_object<core::shader_effect>(AID("se_simple_texture"));
    occ.get_instance_local_set()->map->add_item(*se_simple_texture);

    auto txt_red = core::object_constructor::alloc_empty_object<core::texture>(AID("txt_red"));
    occ.get_instance_local_set()->map->add_item(*txt_red);

    auto root_component = core::object_constructor::alloc_empty_object<core::game_object_component>(
        AID("root_component"));
    occ.get_instance_local_set()->map->add_item(*root_component);

    auto mt_red = core::object_constructor::alloc_empty_object<core::material>(AID("mt_red"));
    occ.get_instance_local_set()->map->add_item(*mt_red);
    mt_red->set_shader_effect(se_simple_texture->as<core::shader_effect>());
    // mt_red->set_base_texture(txt_red->as<model::texture>());

    auto cube_mesh = core::object_constructor::alloc_empty_object<core::mesh>(AID("cube_mesh"));
    occ.get_instance_local_set()->map->add_item(*cube_mesh);

    auto mesh_component = core::object_constructor::alloc_empty_object<core::mesh_component>(
        AID("cube_mesh_component"));
    mesh_component->set_material(mt_red->as<core::material>());
    mesh_component->set_mesh(cube_mesh->as<core::mesh>());
    occ.get_instance_local_set()->map->add_item(*mesh_component);

    core::smart_object* obj = nullptr;
    auto rc = core::object_constructor::object_load(APATH("class/game_objects/cubes_chain.aobj"),
                                                    core::object_load_type::instance_obj, occ, obj,
                                                    dummy_loaded_obj);
    ASSERT_EQ(rc, result_code::ok);
    ASSERT_TRUE(!!obj);

    check_item_in_caches(AID("cubes_chain"), core::architype::game_object, false, true);

    auto game_object = obj->as<core::game_object>();
    ASSERT_TRUE(!!game_object);

    ASSERT_EQ(occ.get_instance_local_set()->objects->get_size(), 10);

    ASSERT_EQ(game_object->get_id(), AID("cubes_chain"));
    ASSERT_EQ(game_object->get_type_id(), AID("mesh_object"));

    {
        auto component = game_object->get_component_at(0U);
        ASSERT_EQ(component->get_id(), AID("cube_chain_root_component"));
        ASSERT_EQ(component->get_type_id(), AID("game_object_component"));
        ASSERT_EQ(component->get_order_idx(), 0);
        ASSERT_EQ(component->get_parent_idx(), core::NO_parent);
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

    rc = core::object_constructor::object_save(*game_object, get_current_workspace() / "save.aobj");
    ASSERT_EQ(rc, result_code::ok);

    utils::path p;
    occ.make_full_path(APATH("class/game_objects/cubes_chain.aobj"), p);

    auto result = utils::file_utils::compare_files(p, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, test_object_miroring)
{
    auto se_simple_texture =
        core::object_constructor::alloc_empty_object<core::shader_effect>(AID("se_simple_texture"));
    occ.get_proto_local_set()->map->add_item(*se_simple_texture);

    auto txt_red = core::object_constructor::alloc_empty_object<core::texture>(AID("txt_red"));
    occ.get_proto_local_set()->map->add_item(*txt_red);

    auto mt_red = core::object_constructor::alloc_empty_object<core::material>(AID("mt_red"));
    occ.get_proto_local_set()->map->add_item(*mt_red);
    mt_red->set_shader_effect(se_simple_texture->as<core::shader_effect>());
    // mt_red->set_base_texture(txt_red->as<model::texture>());

    auto cube_mesh = core::object_constructor::alloc_empty_object<core::mesh>(AID("cube_mesh"));
    occ.get_proto_local_set()->map->add_item(*cube_mesh);

    auto mesh_component = core::object_constructor::alloc_empty_object<core::mesh_component>(
        AID("cube_mesh_component"));
    mesh_component->set_material(mt_red->as<core::material>());
    mesh_component->set_mesh(cube_mesh->as<core::mesh>());
    occ.get_proto_local_set()->map->add_item(*mesh_component);

    auto root_component = core::object_constructor::alloc_empty_object<core::game_object_component>(
        AID("root_component"));
    occ.get_proto_local_set()->map->add_item(*root_component);

    core::smart_object* obj = nullptr;
    auto rc = core::object_constructor::object_load(APATH("class/game_objects/cubes_chain.aobj"),
                                                    core::object_load_type::class_obj, occ, obj,
                                                    dummy_loaded_obj);
    ASSERT_EQ(rc, result_code::ok);
    ASSERT_TRUE(!!obj);

    obj = nullptr;
    rc = core::object_constructor::mirror_object(AID("cubes_chain"), occ, obj, dummy_loaded_obj);

    ASSERT_EQ(rc, result_code::ok);
    ASSERT_TRUE(!!obj);

    check_item_in_caches(AID("cubes_chain"), core::architype::game_object, true, true);
    check_item_in_caches(AID("cube_chain_root_component"), core::architype::component, true, true);
    check_item_in_caches(AID("cubes_chain_cube_mesh_1"), core::architype::component, true, true);
    check_item_in_caches(AID("cubes_chain_cube_mesh_2"), core::architype::component, true, true);
    check_item_in_caches(AID("mt_red"), core::architype::material, true, true);
    check_item_in_caches(AID("se_simple_texture"), core::architype::shader_effect, true, true);

    auto game_object = obj->as<core::game_object>();
    ASSERT_TRUE(!!game_object);

    ASSERT_EQ(occ.get_instance_local_set()->objects->get_size(), 7);

    ASSERT_EQ(game_object->get_id(), AID("cubes_chain"));
    ASSERT_EQ(game_object->get_type_id(), AID("mesh_object"));

    {
        auto component = game_object->get_component_at(0U);
        ASSERT_EQ(component->get_id(), AID("cube_chain_root_component"));
        ASSERT_EQ(component->get_type_id(), AID("game_object_component"));
        ASSERT_EQ(component->get_order_idx(), 0);
        ASSERT_EQ(component->get_parent_idx(), core::NO_parent);
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
}

#endif
