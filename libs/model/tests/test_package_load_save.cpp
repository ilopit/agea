#include "model/object_constructor.h"

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
#include "utils/path.h"

#include "base_test.h"

#define ID(val) ::agea::utils::id::from(val)
using namespace agea;

struct test_load_package : public base_test
{
    void
    TearDown()
    {
        base_test::TearDown();
    }
};

TEST_F(test_load_package, load_package)
{
    auto path = glob::resource_locator::get()->resource(category::packages, "test.apkg");

    model::cache_set class_global_cache;
    model::cache_set instance_global_cache;

    model::package p;
    auto result = model::package::load_package(path, p, class_global_cache.get_ref(),
                                               instance_global_cache.get_ref());
    ASSERT_TRUE(result);

    ASSERT_EQ(p.get_id(), ID("test"));
    ASSERT_EQ(p.get_load_path().str(), path.str());

    auto& cache = p.get_class_cache();
    auto& class_cache = *cache.objects;

    auto all_size = class_cache.get_size();

    uint32_t size = 0;
    size += cache.game_objects->get_size();
    size += cache.materials->get_size();
    size += cache.meshes->get_size();
    size += cache.textures->get_size();
    size += cache.components->get_size();

    ASSERT_EQ(all_size, size);

    auto game_object = cache.game_objects->get_item(ID("test_class_object"));
    ASSERT_TRUE(!!game_object);

    ASSERT_EQ(game_object->get_id(), ID("test_class_object"));
    ASSERT_EQ(game_object->get_type_id(), ID("mesh_object"));

    {
        {
            auto component = game_object->get_component_at(0U);
            ASSERT_EQ(component->get_id(), ID("test_class_object__test_class_root_component"));
            ASSERT_EQ(component->get_type_id(), ID("game_object_component"));
            ASSERT_EQ(component->get_order_idx(), 0U);
            ASSERT_EQ(component->get_parent_idx(), model::NO_parent);
        }
        {
            auto component = game_object->get_component_at(1U);
            ASSERT_EQ(component->get_id(), ID("test_class_object__test_class_component"));
            ASSERT_EQ(component->get_type_id(), ID("component"));
            ASSERT_EQ(component->get_order_idx(), 1U);
            ASSERT_EQ(component->get_parent_idx(), 0U);
        }
    }
    result = model::package::save_package(get_current_workspace(), p);
    ASSERT_TRUE(result);

    result = utils::file_utils::compare_folders(path, get_current_workspace() / "test.apkg");
    ASSERT_TRUE(result);
}
