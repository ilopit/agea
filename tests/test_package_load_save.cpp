#include "model/object_constructor.h"

#include "model/caches/class_object_cache.h"
#include "model/caches/materials_cache.h"
#include "model/caches/textures_cache.h"
#include "model/caches/meshes_cache.h"
#include "model/caches/components_cache.h"
#include "model/caches/game_objects_cache.h"
#include "model/caches/empty_objects_cache.h"
#include "model/caches/cache_set.h"

#include "model/object_construction_context.h"
#include "model/level.h"
#include "model/package.h"
#include "model/game_object.h"

#include "utils/agea_log.h"
#include "utils/file_utils.h"

#include <gtest/gtest.h>

using namespace agea;

struct test_load_package : public testing::Test
{
    void
    SetUp()
    {
        m_resource_locator = glob::resource_locator::create();
        m_game_objects_cache = glob::game_objects_cache::create();
    }

    void
    TearDown()
    {
        m_resource_locator.reset();
    }
    std::unique_ptr<closure<resource_locator>> m_resource_locator;

    std::unique_ptr<closure<model::components_cache>> m_components_cache;
    std::unique_ptr<closure<model::game_objects_cache>> m_game_objects_cache;
};

TEST_F(test_load_package, load_package)
{
    auto path = glob::resource_locator::get()->resource(category::packages, "test.apkg");

    model::cache_set global_cache;

    model::package p;
    auto result = model::package::load_package(path, p, global_cache.get_ref());
    ASSERT_TRUE(result);

    ASSERT_EQ(p.get_id(), "test.apkg");
    ASSERT_EQ(p.get_path(), path);

    auto& cache = p.get_cache();

    auto& class_cache = p.get_cache().objects;

    auto all_size = class_cache->get_size();

    uint32_t size = 0;
    size += cache.game_objects->get_size();
    size += cache.materials->get_size();
    size += cache.meshes->get_size();
    size += cache.textures->get_size();
    size += cache.components->get_size();

    ASSERT_EQ(all_size, size);

    auto game_object = cache.game_objects->get_item("test_object");
    ASSERT_TRUE(!!game_object);

    ASSERT_EQ(game_object->get_id(), "test_object");
    ASSERT_EQ(game_object->get_type_id(), "mesh_object");

    {
        {
            auto component = game_object->get_component_at(0U);
            ASSERT_EQ(component->get_id(), "test_obj/test_root_component");
            ASSERT_EQ(component->get_type_id(), "game_object_component");
            ASSERT_EQ(component->get_order_idx(), 0U);
            ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
        }
        {
            auto component = game_object->get_component_at(1U);
            ASSERT_EQ(component->get_id(), "test_obj/test_component_a");
            ASSERT_EQ(component->get_type_id(), "component");
            ASSERT_EQ(component->get_order_idx(), 1U);
            ASSERT_EQ(component->get_parent_idx(), 0U);
        }
        {
            auto component = game_object->get_component_at(2U);
            ASSERT_EQ(component->get_id(), "test_obj/test_component_b");
            ASSERT_EQ(component->get_type_id(), "component");
            ASSERT_EQ(component->get_order_idx(), 2U);
            ASSERT_EQ(component->get_parent_idx(), 0U);
        }
        {
            auto component = game_object->get_component_at(3U);
            ASSERT_EQ(component->get_id(), "test_obj/test_component_a_2");
            ASSERT_EQ(component->get_type_id(), "game_object_component");
            ASSERT_EQ(component->get_order_idx(), 3U);
            ASSERT_EQ(component->get_parent_idx(), 2U);
        }
        {
            auto component = game_object->get_component_at(4U);
            ASSERT_EQ(component->get_id(), "test_obj/test_component_b_2");
            ASSERT_EQ(component->get_type_id(), "component");
            ASSERT_EQ(component->get_order_idx(), 4U);
            ASSERT_EQ(component->get_parent_idx(), 2U);
        }
    }
    result = model::package::save_package("test.apkg", p);
    ASSERT_TRUE(result);

    result = file_utils::compare_folders(path, "test.apkg");
    ASSERT_TRUE(result);
}
