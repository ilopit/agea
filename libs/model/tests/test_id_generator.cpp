#include "base_test.h"

#include <model/id_generator.h>
#include <model/caches/objects_cache.h>
#include <model/caches/objects_cache.h>
#include <model/caches/caches_map.h>
#include <model/smart_object.h>
#include <model/object_constructor.h>

#include <utils/singleton_registry.h>

#include <gtest/gtest.h>

using namespace agea;

struct test_id_generator : base_test
{
    void
    SetUp()
    {
        base_test::SetUp();
        glob::id_generator::create(m_reg);
        glob::class_objects_cache::create_ref(&m_class_cache);
        glob::objects_cache::create_ref(&m_cache);
    }

    singleton_registry m_reg;
    model::objects_cache m_class_cache;
    model::objects_cache m_cache;
};

TEST_F(test_id_generator, generate_components_ids)
{
    auto new_id = glob::id_generator::getr().generate(AID("foo"), AID("bar"));
    ASSERT_EQ(new_id, AID("foo/bar#2"));

    new_id = glob::id_generator::getr().generate(AID("foo"), AID("bar"));
    ASSERT_EQ(new_id, AID("foo/bar#3"));

    new_id = glob::id_generator::getr().generate(AID("foo"), AID("bar#3"));
    ASSERT_EQ(new_id, AID("foo/bar#4"));

    new_id = glob::id_generator::getr().generate(AID("foo"), AID("bar#5"));
    ASSERT_EQ(new_id, AID("foo/bar#5"));

    auto m = model::object_constructor::create_empty_object<model::smart_object>(AID("foo/bar#6"));
    m_class_cache.add_item(*m);

    new_id = glob::id_generator::getr().generate(AID("foo"), AID("bar#5"));
    ASSERT_EQ(new_id, AID("foo/bar#7"));
}

TEST_F(test_id_generator, generate_obj_ids)
{
    auto new_id = glob::id_generator::getr().generate(AID("bar"));
    ASSERT_EQ(new_id, AID("bar#2"));

    new_id = glob::id_generator::getr().generate(AID("bar"));
    ASSERT_EQ(new_id, AID("bar#3"));

    new_id = glob::id_generator::getr().generate(AID("bar#3"));
    ASSERT_EQ(new_id, AID("bar#4"));

    new_id = glob::id_generator::getr().generate(AID("bar#5"));
    ASSERT_EQ(new_id, AID("bar#5"));

    auto m = model::object_constructor::create_empty_object<model::smart_object>(AID("bar#6"));
    m_class_cache.add_item(*m);

    new_id = glob::id_generator::getr().generate(AID("bar#5"));
    ASSERT_EQ(new_id, AID("bar#7"));
}