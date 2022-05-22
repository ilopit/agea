#include "model/object_constructor.h"

#include "model/caches/class_object_cache.h"
#include "model/object_construction_context.h"
#include "model/level.h"
#include "model/level_constructor.h"

#include "utils/agea_log.h"

#include <gtest/gtest.h>

using namespace agea;

struct load_save_test : public testing::Test
{
    void
    SetUp()
    {
        m_resource_locator = glob::resource_locator::create();
        m_coc = glob::class_objects_cache::create();
    }

    void
    TearDown()
    {
        m_resource_locator.reset();
        m_coc.reset();
    }

    std::unique_ptr<closure<resource_locator>> m_resource_locator;
    std::unique_ptr<closure<model::class_objects_cache>> m_coc;
};

TEST_F(load_save_test, DISABLED_load_save_component)
{
    auto path =
        glob::resource_locator::get()->resource(category::all, "objects/test/test_object.aobj");

    model::object_constructor_context occ;
    model::object_constructor::class_object_load(path, occ);
}

TEST_F(load_save_test, DISABLED_load_save_object)
{
    std::vector<const char*> to_load{"test/test_root_component.acom", "test/test_component_a.acom",
                                     "test/test_component_b.acom", "test/test_component_a_2.acom",
                                     "test/test_component_b_2.acom"};

    model::object_constructor_context occ;
    for (auto name : to_load)
    {
        auto path = glob::resource_locator::get()->resource(category::all, name);
        ASSERT_FALSE(path.empty());

        model::object_constructor::class_object_load(path, occ);
    }

    auto path = glob::resource_locator::get()->resource(category::all, "test/test_object.aobj");

    auto obj = model::object_constructor::class_object_load(path, occ);

    obj = model::object_constructor::object_clone_create(*obj, "new", occ);
}

TEST_F(load_save_test, load_save_level)
{
    model::level l;

    auto path = glob::resource_locator::get()->resource(category::all, "test/test_level.alvl");

    model::level_constructor::load_level_path(l, path);

    int i = 2;
}