#include "model/object_constructor.h"

#include "model/caches/class_object_cache.h"
#include "model/object_construction_context.h"
#include "model/level.h"
#include "model/level_constructor.h"

#include "utils/agea_log.h"

#include <gtest/gtest.h>

using namespace agea;

struct test_load_level : public testing::Test
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

TEST_F(test_load_level, load_level)
{
    model::level l;

    auto path = glob::resource_locator::get()->resource(category::all, "test/test_level.alvl");

    model::level_constructor::load_level_path(l, path);
}