#include "model/object_constructor.h"

#include "model/caches/class_object_cache.h"
#include "model/caches/game_objects_cache.h"
#include "model/caches/empty_objects_cache.h"

#include "model/object_construction_context.h"

#include "model/level.h"
#include "model/level_constructor.h"
#include "model/game_object.h"

#include "utils/file_utils.h"

#include "serialization/serialization.h"

#include "utils/agea_log.h"

#include <gtest/gtest.h>

#define ID(val) ::agea::core::id::from(val)

using namespace agea;

struct test_object_constructor : public testing::Test
{
    void
    SetUp()
    {
        m_resource_locator = glob::resource_locator::create();
    }

    void
    TearDown()
    {
        m_resource_locator.reset();
    }

    std::unique_ptr<closure<resource_locator>> m_resource_locator;
};

bool
is_from_EO_cache(model::smart_object* obj)
{
    return glob::empty_objects_cache::get()->get(obj->get_type_id()) == obj;
}

TEST_F(test_object_constructor, load_component)
{
    auto object_path =
        glob::resource_locator::get()->resource(category::all, "test/test_component_a_2.acom");

    ASSERT_FALSE(object_path.empty());

    model::cache_set l, g;
    model::line_cache objs;

    model::object_constructor_context occ(g.get_ref(), l.get_ref(), &objs);
    auto obj = model::object_constructor::class_object_load(object_path, occ);
    ASSERT_TRUE(!!obj);

    auto component = obj->as<model::game_object_component>();

    ASSERT_EQ(component->get_id(), ID("test_component_a_2"));
    ASSERT_EQ(component->get_type_id(), ID("game_object_component"));
    ASSERT_EQ(component->get_order_idx(), model::NO_index);
    ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
    ASSERT_EQ(component->is_renderable(), false);

    ASSERT_FALSE(is_from_EO_cache(component));

    utils::path result_path("result");
    auto result = model::object_constructor::class_object_save(*obj, result_path);
    ASSERT_TRUE(result);

    result = utils::file_utils::compare_files(object_path, result_path);
    ASSERT_TRUE(result);
}

TEST_F(test_object_constructor, load_and_save_object)
{
    const std::vector<std::string> to_load{
        "test/test_root_component.acom", "test/test_component_a.acom", "test/test_component_b.acom",
        "test/test_component_a_2.acom", "test/test_component_b_2.acom"};

    model::cache_set l, g;
    model::line_cache objs;

    model::object_constructor_context occ(g.get_ref(), l.get_ref(), &objs);
    for (const auto& name : to_load)
    {
        auto path = glob::resource_locator::get()->resource(category::all, name);
        ASSERT_FALSE(path.empty());

        auto obj = model::object_constructor::class_object_load(path, occ);
        ASSERT_TRUE(!!obj);
    }

    auto object_path =
        glob::resource_locator::get()->resource(category::all, "test/test_object.aobj");
    ASSERT_FALSE(object_path.empty());

    auto obj = model::object_constructor::class_object_load(object_path, occ);
    ASSERT_TRUE(!!obj);

    ASSERT_EQ(occ.m_local_set.objects->get_size(), to_load.size() + to_load.size() + 1);

    auto game_object = obj->as<model::game_object>();
    ASSERT_TRUE(!!game_object);

    ASSERT_EQ(game_object->get_id(), ID("test_object"));
    ASSERT_EQ(game_object->get_type_id(), ID("mesh_object"));

    {
        auto component = game_object->get_component_at(0U);
        ASSERT_EQ(component->get_id(), ID("test_obj/test_root_component"));
        ASSERT_EQ(component->get_type_id(), ID("game_object_component"));
        ASSERT_EQ(component->get_order_idx(), 0U);
        ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->get_component_at(1U);
        ASSERT_EQ(component->get_id(), ID("test_obj/test_component_a"));
        ASSERT_EQ(component->get_type_id(), ID("component"));
        ASSERT_EQ(component->get_order_idx(), 1U);
        ASSERT_EQ(component->get_parent_idx(), 0U);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->get_component_at(2U);
        ASSERT_EQ(component->get_id(), ID("test_obj/test_component_b"));
        ASSERT_EQ(component->get_type_id(), ID("component"));
        ASSERT_EQ(component->get_order_idx(), 2U);
        ASSERT_EQ(component->get_parent_idx(), 0U);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->get_component_at(3U);
        ASSERT_EQ(component->get_id(), ID("test_obj/test_component_a_2"));
        ASSERT_EQ(component->get_type_id(), ID("game_object_component"));
        ASSERT_EQ(component->get_order_idx(), 3U);
        ASSERT_EQ(component->get_parent_idx(), 2U);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->get_component_at(4U);
        ASSERT_EQ(component->get_id(), ID("test_obj/test_component_b_2"));
        ASSERT_EQ(component->get_type_id(), ID("component"));
        ASSERT_EQ(component->get_order_idx(), 4U);
        ASSERT_EQ(component->get_parent_idx(), 2U);
        ASSERT_FALSE(is_from_EO_cache(component));
    }

    utils::path result_path("result");
    auto result = model::object_constructor::class_object_save(*game_object, result_path);
    ASSERT_TRUE(result);

    result = utils::file_utils::compare_files(object_path, result_path);
    ASSERT_TRUE(result);
}
