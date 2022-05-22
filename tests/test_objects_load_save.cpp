#include "model/object_constructor.h"

#include "model/caches/class_object_cache.h"
#include "model/caches/objects_cache.h"
#include "model/caches/empty_objects_cache.h"

#include "model/object_construction_context.h"

#include "model/level.h"
#include "model/level_constructor.h"
#include "model/game_object.h"

#include "utils/agea_log.h"

#include <gtest/gtest.h>

using namespace agea;

struct test_load_objects : public testing::Test
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
    return glob::empty_objects_cache::get()->get(obj->type_id()) == obj;
}

TEST_F(test_load_objects, load_component)
{
    auto path =
        glob::resource_locator::get()->resource(category::all, "test/test_component_a_2.acom");

    ASSERT_FALSE(path.empty());

    model::object_constructor_context occ;
    auto obj = model::object_constructor::class_object_load(path, occ);
    ASSERT_TRUE(!!obj);

    auto component = obj->as<model::component>();

    ASSERT_EQ(component->id(), "test_component_a_2");
    ASSERT_EQ(component->type_id(), "component");
    ASSERT_EQ(component->m_order_idx, model::NO_index);
    ASSERT_EQ(component->m_parent_idx, model::NO_parent);

    auto eoc = glob::empty_objects_cache::get();
    auto empty_obj = eoc->get(component->type_id());

    ASSERT_NE(empty_obj, obj);
}

TEST_F(test_load_objects, load_object)
{
    const std::vector<const char*> to_load{
        "test/test_root_component.acom", "test/test_component_a.acom", "test/test_component_b.acom",
        "test/test_component_a_2.acom", "test/test_component_b_2.acom"};

    model::object_constructor_context occ;
    for (auto name : to_load)
    {
        auto path = glob::resource_locator::get()->resource(category::all, name);
        ASSERT_FALSE(path.empty());

        auto obj = model::object_constructor::class_object_load(path, occ);
        ASSERT_TRUE(!!obj);
    }

    auto path = glob::resource_locator::get()->resource(category::all, "test/test_object.aobj");
    ASSERT_FALSE(path.empty());

    auto obj = model::object_constructor::class_object_load(path, occ);
    ASSERT_TRUE(!!obj);

    ASSERT_EQ(occ.class_obj_cache->size(), to_load.size() + to_load.size() + 1);
    ASSERT_EQ(occ.instance_obj_cache->size(), 0U);
    ASSERT_EQ(occ.temporary_obj_cache.size(), 0U);

    auto game_object = obj->as<model::game_object>();
    ASSERT_TRUE(!!game_object);

    ASSERT_EQ(game_object->id(), "test_object");
    ASSERT_EQ(game_object->type_id(), "mesh_object");

    {
        auto component = game_object->component_at(0U);
        ASSERT_EQ(component->id(), "test_obj/test_root_component");
        ASSERT_EQ(component->type_id(), "game_object_component");
        ASSERT_EQ(component->m_order_idx, 0U);
        ASSERT_EQ(component->m_parent_idx, model::NO_parent);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->component_at(1U);
        ASSERT_EQ(component->id(), "test_obj/test_component_a");
        ASSERT_EQ(component->type_id(), "component");
        ASSERT_EQ(component->m_order_idx, 1U);
        ASSERT_EQ(component->m_parent_idx, 0U);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->component_at(2U);
        ASSERT_EQ(component->id(), "test_obj/test_component_b");
        ASSERT_EQ(component->type_id(), "component");
        ASSERT_EQ(component->m_order_idx, 2U);
        ASSERT_EQ(component->m_parent_idx, 0U);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->component_at(3U);
        ASSERT_EQ(component->id(), "test_obj/test_component_a_2");
        ASSERT_EQ(component->type_id(), "component");
        ASSERT_EQ(component->m_order_idx, 3U);
        ASSERT_EQ(component->m_parent_idx, 2U);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
    {
        auto component = game_object->component_at(4U);
        ASSERT_EQ(component->id(), "test_obj/test_component_b_2");
        ASSERT_EQ(component->type_id(), "component");
        ASSERT_EQ(component->m_order_idx, 4U);
        ASSERT_EQ(component->m_parent_idx, 2U);
        ASSERT_FALSE(is_from_EO_cache(component));
    }
}
