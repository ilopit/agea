#include "model/object_constructor.h"

#include "model/caches/class_object_cache.h"
#include "model/caches/materials_cache.h"
#include "model/caches/textures_cache.h"
#include "model/caches/meshes_cache.h"
#include "model/package_manager.h"
#include "model/object_construction_context.h"
#include "model/level.h"
#include "model/level_constructor.h"
#include "model/game_object.h"

#include "utils/agea_log.h"
#include "utils/file_utils.h"

#include <gtest/gtest.h>

#define ID(val) ::agea::core::id::from(val)

using namespace agea;

struct test_load_level : public testing::Test
{
    void
    SetUp()
    {
        m_resource_locator = glob::resource_locator::create();
        m_package_manager = glob::package_manager::create();
        m_cache_set = glob::cache_set::create();
    }

    void
    TearDown()
    {
        m_resource_locator.reset();
    }

    std::unique_ptr<closure<model::package_manager>> m_package_manager;
    std::unique_ptr<closure<resource_locator>> m_resource_locator;
    std::unique_ptr<closure<model::cache_set>> m_cache_set;
};

TEST_F(test_load_level, load_level)
{
    model::level l;

    auto path = glob::resource_locator::get()->resource(category::all, "test/test_level.alvl");

    ASSERT_TRUE(!path.empty());
    auto result =
        model::level_constructor::load_level_path(l, path, glob::cache_set::getr().get_ref());
    ASSERT_TRUE(result);

    {
        auto obj = l.find_game_object(ID("instance_test_obj_1"));
        ASSERT_TRUE(obj);

        auto comps = obj->get_components();

        ASSERT_EQ(obj->get_id(), ID("instance_test_obj_1"));
        ASSERT_EQ(comps[0]->get_id(), ID("instance_test_comp_1"));
        auto game_comp = comps[0]->as<model::game_object_component>();

        const auto expected_pos = glm::vec3{-10.f, -10.f, 0.f};
        ASSERT_EQ(game_comp->get_position(), expected_pos);

        const auto expected_rot = glm::vec3{1.f, 1.f, 1.f};
        ASSERT_EQ(game_comp->get_rotation(), expected_rot);

        const auto expected_scale = glm::vec3{5.f, 5.f, 5.f};
        ASSERT_EQ(game_comp->get_scale(), expected_scale);

        ASSERT_EQ(comps[1]->get_id(), ID("instance_test_obj_1/test_component_a"));
        ASSERT_EQ(comps[2]->get_id(), ID("instance_test_obj_1/test_component_b"));
        ASSERT_EQ(comps[3]->get_id(), ID("instance_test_obj_1/test_component_a_2"));
        ASSERT_EQ(comps[4]->get_id(), ID("instance_test_obj_1/test_component_b_2"));
    }

    {
        auto obj = l.find_game_object(ID("instance_test_obj_2"));
        ASSERT_TRUE(obj);

        auto comps = obj->get_components();

        ASSERT_EQ(obj->get_id(), ID("instance_test_obj_2"));
        ASSERT_EQ(comps[0]->get_id(), ID("instance_test_comp_2"));
        auto game_comp = comps[0]->as<model::game_object_component>();

        const auto expected_pos = glm::vec3{-5.f, -10.f, 0.f};
        ASSERT_EQ(game_comp->get_position(), expected_pos);

        const auto expected_rot = glm::vec3{2.f, 3.f, 4.f};
        ASSERT_EQ(game_comp->get_rotation(), expected_rot);

        const auto expected_scale = glm::vec3{3.f, 3.f, 3.f};
        ASSERT_EQ(game_comp->get_scale(), expected_scale);

        ASSERT_EQ(comps[1]->get_id(), ID("instance_test_obj_2/test_component_a"));
        ASSERT_EQ(comps[2]->get_id(), ID("instance_test_obj_2/test_component_b"));
        ASSERT_EQ(comps[3]->get_id(), ID("instance_test_obj_2/test_component_a_2"));
        ASSERT_EQ(comps[4]->get_id(), ID("instance_test_obj_2/test_component_b_2"));
    }

    utils::path save_path("result");

    model::level_constructor::save_level(l, save_path);

    result = utils::file_utils::compare_files(save_path, path);
    ASSERT_TRUE(result);
}