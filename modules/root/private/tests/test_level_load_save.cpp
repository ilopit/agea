#if 0

#include "model/object_constructor.h"

#include "model/caches/materials_cache.h"
#include "model/caches/textures_cache.h"
#include "model/caches/meshes_cache.h"
#include "model/caches/components_cache.h"
#include "model/caches/objects_cache.h"

#include <model/package_manager.h>
#include <model/object_construction_context.h>
#include <model/level.h>
#include <model/level_constructor.h>
#include <model/game_object.h>

#include <utils/agea_log.h>
#include <utils/file_utils.h>

#include <gtest/gtest.h>

#define ID(val) ::agea::utils::id::from(val)

#include "base_test.h"

using namespace agea;

struct test_load_level : public base_test
{
    void
    SetUp()
    {
        base_test::SetUp();

        m_package_manager = glob::package_manager::create();

        glob::init_global_caches(m_class_objects_cache_set, m_objects_cache_set);
    }

    void
    TearDown()
    {
        m_resource_locator.reset();

        base_test::TearDown();
    }

    agea::singletone_autodeleter m_package_manager;
    agea::singletone_autodeleter m_resource_locator;
    agea::singletone_autodeleter m_objects_cache_set;
    agea::singletone_autodeleter m_class_objects_cache_set;
};

TEST_F(test_load_level, load_level)
{
    model::level l;

    auto path = glob::resource_locator::get()->resource(category::levels, "test.alvl");

    ASSERT_TRUE(!path.empty());
    auto result = model::level_constructor::load_level_path(
        l, path, glob::class_objects_cache_set::getr().get_ref(),
        glob::objects_cache_set::getr().get_ref());
    ASSERT_TRUE(result);

    {
        auto obj = l.find_game_object(ID("test_instanded_object_1"));
        ASSERT_TRUE(obj);

        auto comps = obj->get_components();

        ASSERT_EQ(obj->get_id(), ID("test_instanded_object_1"));
        ASSERT_EQ(comps[0]->get_id(), ID("test_instanded_object_1__test_root_component"));
        //         auto game_comp = comps[0]->as<model::game_object_component>();
        //
        //         const auto expected_pos = glm::vec3{-10.f, -10.f, 0.f};
        //         ASSERT_EQ(game_comp->get_position(), expected_pos);
        //
        //         const auto expected_rot = glm::vec3{1.f, 1.f, 1.f};
        //         ASSERT_EQ(game_comp->get_rotation(), expected_rot);
        //
        //         const auto expected_scale = glm::vec3{5.f, 5.f, 5.f};
        //         ASSERT_EQ(game_comp->get_scale(), expected_scale);

        ASSERT_EQ(comps[1]->get_id(), ID("test_instanded_object_1__test_component"));
    }

    {
        auto obj = l.find_game_object(ID("test_instanded_object_2"));
        ASSERT_TRUE(obj);

        auto comps = obj->get_components();

        ASSERT_EQ(obj->get_id(), ID("test_instanded_object_2"));
        ASSERT_EQ(comps[0]->get_id(), ID("test_instanded_object_2__test_root_component"));
        auto game_comp = comps[0]->as<model::game_object_component>();

        //         const auto expected_pos = glm::vec3{-5.f, -10.f, 0.f};
        //         ASSERT_EQ(game_comp->get_position(), expected_pos);
        //
        //         const auto expected_rot = glm::vec3{2.f, 3.f, 4.f};
        //         ASSERT_EQ(game_comp->get_rotation(), expected_rot);
        //
        //         const auto expected_scale = glm::vec3{3.f, 3.f, 3.f};
        //         ASSERT_EQ(game_comp->get_scale(), expected_scale);

        ASSERT_EQ(comps[1]->get_id(), ID("test_instanded_object_2__test_component"));
    }

    model::level_constructor::save_level(l, get_current_workspace());

    result = utils::file_utils::compare_folders(get_current_workspace() / "test.alvl", path);
    ASSERT_TRUE(result);
}

#endif
