#include "base_test.h"

#include <core/id_generator.h>

#include <core/caches/caches_map.h>
#include <core/object_constructor.h>

#include <packages/root/smart_object.h>

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
    }

    singleton_registry m_reg;
    core::objects_cache m_class_cache;
    core::objects_cache m_cache;
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

    auto m = core::object_constructor::alloc_empty_object<root::smart_object>(AID("foo/bar#6"));
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

    auto m = core::object_constructor::alloc_empty_object<root::smart_object>(AID("bar#6"));
    m_class_cache.add_item(*m);

    new_id = glob::id_generator::getr().generate(AID("bar#5"));
    ASSERT_EQ(new_id, AID("bar#7"));
}
