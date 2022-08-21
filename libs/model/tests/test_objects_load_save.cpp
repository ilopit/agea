#include "model/object_constructor.h"

#include "model/caches/game_objects_cache.h"
#include "model/caches/empty_objects_cache.h"

#include "model/object_construction_context.h"

#include "model/level.h"
#include "model/level_constructor.h"
#include "model/game_object.h"

#include "utils/file_utils.h"

#include "serialization/serialization.h"

#include "utils/agea_log.h"
#include "base_test.h"

#include <gtest/gtest.h>

#define ID(val) ::agea::utils::id::from(val)

using namespace agea;

struct test_object_constructor : base_test
{
    void
    SetUp()
    {
        base_test::SetUp();

        {
            auto prefix = glob::resource_locator::get()->resource(category::all, "packages/cube_chain.apgk")
        }

        occ = model::object_constructor_context(
            utils::path(), global_class_objects_cs.get_ref(), local_class_objects_cs.get_ref(),
            global_objects_cs.get_ref(), local_objects_cs.get_ref(), &objs);
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
    model::line_cache objs;
    model::object_constructor_context occ;
    agea::singletone_autodeleter m_resource_locator;
};

bool
is_from_EO_cache(model::smart_object* obj)
{
    return glob::empty_objects_cache::get()->get(obj->get_type_id()) == obj;
}

TEST_F(test_object_constructor, load_and_save_class_component)
{
    auto object_path = glob::resource_locator::get()->resource(
        category::all, "packages/test.apkg/class/components/test_class_root_component.aobj");

    ASSERT_FALSE(object_path.empty());

    auto obj = model::object_constructor::class_object_load(object_path, occ);
    ASSERT_TRUE(!!obj);

    auto component = obj->as<model::game_object_component>();

    ASSERT_EQ(component->get_id(), ID("test_class_root_component"));
    ASSERT_EQ(component->get_type_id(), ID("game_object_component"));
    ASSERT_EQ(component->get_order_idx(), model::NO_index);
    ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
    ASSERT_EQ(component->is_renderable(), false);

    ASSERT_FALSE(is_from_EO_cache(component));

    check_item_in_caches(ID("test_class_root_component"), model::architype::component, true);

    ASSERT_EQ(objs.get_size(), 1U);

    auto result =
        model::object_constructor::class_object_save(*obj, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);

    result = utils::file_utils::compare_files(object_path, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, load_and_save_instanced_component)
{
    auto class_comonent_path = glob::resource_locator::get()->resource(
        category::all, "packages/test.apkg/class/components/test_class_root_component.aobj");
    ASSERT_FALSE(class_comonent_path.empty());

    auto component_path = glob::resource_locator::get()->resource(
        category::all,
        "packages/test.apkg/class/components/"
        "test_class_object__test_class_root_component.aobj");
    ASSERT_FALSE(component_path.empty());

    auto obj = model::object_constructor::class_object_load(class_comonent_path, occ);
    ASSERT_TRUE(!!obj);
    obj = model::object_constructor::instance_object_load(component_path, occ);
    ASSERT_TRUE(!!obj);

    auto component = obj->as<model::game_object_component>();

    ASSERT_EQ(component->get_id(), ID("test_class_object__test_class_root_component"));
    ASSERT_EQ(component->get_type_id(), ID("game_object_component"));
    ASSERT_EQ(component->get_order_idx(), 0);
    ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
    ASSERT_EQ(component->is_renderable(), false);
    ASSERT_TRUE(component->get_class_obj());

    ASSERT_FALSE(is_from_EO_cache(component));

    check_item_in_caches(ID("test_class_object__test_class_root_component"),
                         model::architype::component, false);

    ASSERT_EQ(objs.get_size(), 2U);

    auto result = model::object_constructor::instance_object_save(
        *obj, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);

    result =
        utils::file_utils::compare_files(component_path, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, load_and_save_component)
{
    auto component_path = glob::resource_locator::get()->resource(
        category::all, "packages/test.apkg/class/components/test_class_root_component.aobj");
    ASSERT_FALSE(component_path.empty());

    auto obj = model::object_constructor::instance_object_load(component_path, occ);
    ASSERT_TRUE(!!obj);

    auto component = obj->as<model::game_object_component>();

    ASSERT_EQ(component->get_id(), ID("test_class_root_component"));
    ASSERT_EQ(component->get_type_id(), ID("game_object_component"));
    ASSERT_EQ(component->get_order_idx(), model::NO_index);
    ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
    ASSERT_EQ(component->is_renderable(), false);

    ASSERT_FALSE(is_from_EO_cache(component));
    check_item_in_caches(ID("test_class_root_component"), model::architype::component, false);

    ASSERT_EQ(objs.get_size(), 1U);

    auto result = model::object_constructor::instance_object_save(
        *obj, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);

    result =
        utils::file_utils::compare_files(component_path, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

/* ====================================================================================== */
TEST_F(test_object_constructor, load_and_save_class_object)
{
    for (const auto& name : k_components_to_load)
    {
        auto path = glob::resource_locator::get()->resource(category::all, name);
        ASSERT_FALSE(path.empty());

        auto obj = model::object_constructor::class_object_load(path, occ);
        ASSERT_TRUE(!!obj);
    }

    auto object_path = glob::resource_locator::get()->resource(
        category::all, "packages/test.apkg/class/game_objects/test_class_object.aobj");
    ASSERT_FALSE(object_path.empty());

    auto obj = model::object_constructor::class_object_load(object_path, occ);
    ASSERT_TRUE(!!obj);

    check_item_in_caches(ID("test_class_object"), model::architype::game_object, true);

    //     ASSERT_EQ(objs.get_size(), k_components_to_load.size() + k_components_to_load.size() +
    //     1); ASSERT_EQ(occ.m_class_local_set.objects->get_size(),
    //               k_components_to_load.size() + k_components_to_load.size() + 1);

    auto game_object = obj->as<model::game_object>();
    ASSERT_TRUE(!!game_object);

    ASSERT_EQ(game_object->get_id(), ID("test_class_object"));
    ASSERT_EQ(game_object->get_type_id(), ID("mesh_object"));

    {
        auto component = game_object->get_component_at(0U);
        ASSERT_EQ(component->get_id(), ID("test_class_object__test_class_root_component"));
        ASSERT_EQ(component->get_type_id(), ID("game_object_component"));
        ASSERT_EQ(component->get_order_idx(), 0U);
        ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->get_component_at(1U);
        ASSERT_EQ(component->get_id(), ID("test_class_object__test_class_component"));
        ASSERT_EQ(component->get_type_id(), ID("component"));
        ASSERT_EQ(component->get_order_idx(), 1U);
        ASSERT_EQ(component->get_parent_idx(), 0U);
        ASSERT_FALSE(is_from_EO_cache(component));
    }

    auto result = model::object_constructor::class_object_save(
        *game_object, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);

    result = utils::file_utils::compare_files(object_path, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, load_and_save_instanced_game_object)
{
    for (const auto& name : k_components_to_load)
    {
        auto path = glob::resource_locator::get()->resource(category::all, name);
        ASSERT_FALSE(path.empty());

        auto obj = model::object_constructor::class_object_load(path, occ);
        ASSERT_TRUE(!!obj);
    }

    for (const auto& name :
         {"levels/test.alvl/components/test_instanded_object_1__test_component.aobj",
          "levels/test.alvl/components/test_instanded_object_1__test_root_component.aobj"})
    {
        auto path = glob::resource_locator::get()->resource(category::all, name);
        ASSERT_FALSE(path.empty());

        auto obj = model::object_constructor::instance_object_load(path, occ);
        ASSERT_TRUE(!!obj);
    }

    auto object_path = glob::resource_locator::get()->resource(
        category::all, "packages/test.apkg/class/game_objects/test_class_object.aobj");
    ASSERT_FALSE(object_path.empty());

    auto obj = model::object_constructor::class_object_load(object_path, occ);
    ASSERT_TRUE(!!obj);

    object_path = glob::resource_locator::get()->resource(
        category::all, "levels/test.alvl/game_objects/test_instanded_object_1.aobj");
    ASSERT_FALSE(object_path.empty());

    obj = model::object_constructor::instance_object_load(object_path, occ);
    ASSERT_TRUE(!!obj);

    check_item_in_caches(ID("test_instanded_object_1"), model::architype::game_object, false);

    //     ASSERT_EQ(objs.get_size(), k_components_to_load.size() + k_components_to_load.size() +
    //                                    k_components_to_load.size() + 2);
    //     ASSERT_EQ(occ.m_class_local_set.objects->get_size(),
    //               k_components_to_load.size() + k_components_to_load.size() + 1);
    //     ASSERT_EQ(occ.m_instance_local_set.objects->get_size(), k_components_to_load.size() + 1);

    auto game_object = obj->as<model::game_object>();
    ASSERT_TRUE(!!game_object);

    ASSERT_EQ(game_object->get_id(), ID("test_instanded_object_1"));
    ASSERT_EQ(game_object->get_type_id(), ID("mesh_object"));

    {
        auto component = game_object->get_component_at(0U);
        ASSERT_EQ(component->get_id(), ID("test_instanded_object_1__test_root_component"));
        ASSERT_EQ(component->get_type_id(), ID("game_object_component"));
        ASSERT_EQ(component->get_order_idx(), 0U);
        ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->get_component_at(1U);
        ASSERT_EQ(component->get_id(), ID("test_instanded_object_1__test_component"));
        ASSERT_EQ(component->get_type_id(), ID("component"));
        ASSERT_EQ(component->get_order_idx(), 1U);
        ASSERT_EQ(component->get_parent_idx(), 0U);
        ASSERT_FALSE(is_from_EO_cache(component));
    }

    auto result = model::object_constructor::instance_object_save(
        *game_object, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);

    result = utils::file_utils::compare_files(object_path, get_current_workspace() / "save.aobj");
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, DISABLED_load_and_save_game_object)
{
    for (const auto& name : k_components_to_load)
    {
        auto path = glob::resource_locator::get()->resource(category::all, name);
        ASSERT_FALSE(path.empty());

        auto obj = model::object_constructor::class_object_load(path, occ);
        ASSERT_TRUE(!!obj);
    }

    auto object_path = glob::resource_locator::get()->resource(
        category::all, "packages/test.apkg/instance/game_objects/test_game_object.aobj");
    ASSERT_FALSE(object_path.empty());

    auto obj = model::object_constructor::instance_object_load(object_path, occ);
    ASSERT_TRUE(!!obj);

    check_item_in_caches(ID("test_game_object"), model::architype::game_object, false);

    //     ASSERT_EQ(objs.get_size(), k_components_to_load.size() + k_components_to_load.size() +
    //     1); ASSERT_EQ(occ.m_class_local_set.objects->get_size(), k_components_to_load.size());
    //     ASSERT_EQ(occ.m_instance_local_set.objects->get_size(), k_components_to_load.size() + 1);

    auto game_object = obj->as<model::game_object>();
    ASSERT_TRUE(!!game_object);

    ASSERT_EQ(game_object->get_id(), ID("test_game_object"));
    ASSERT_EQ(game_object->get_type_id(), ID("mesh_object"));

    {
        auto component = game_object->get_component_at(0U);
        ASSERT_EQ(component->get_id(), ID("test_game_object/test_root_component"));
        ASSERT_EQ(component->get_type_id(), ID("game_object_component"));
        ASSERT_EQ(component->get_order_idx(), 0U);
        ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->get_component_at(1U);
        ASSERT_EQ(component->get_id(), ID("test_game_object/test_component_a"));
        ASSERT_EQ(component->get_type_id(), ID("component"));
        ASSERT_EQ(component->get_order_idx(), 1U);
        ASSERT_EQ(component->get_parent_idx(), 0U);
        ASSERT_FALSE(is_from_EO_cache(component));
    }

    utils::path result_path("result");
    auto result = model::object_constructor::instance_object_save(*game_object, result_path);
    ASSERT_TRUE(result);

    result = utils::file_utils::compare_files(object_path, result_path);
    ASSERT_TRUE(result);
}
