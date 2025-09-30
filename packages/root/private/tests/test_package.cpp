#include "base_test.h"

#include <core/reflection/reflection_type.h>
#include <core/id_generator.h>

#include <packages/root/package.h>

using namespace agea;
using namespace core;
using namespace root;

struct test_root_package : public testing::Test
{
    void
    SetUp()
    {
        glob::id_generator::create(m_registry);
        glob::reflection_type_registry::create(m_registry);
    }

    void
    TearDown()
    {
        m_registry = {};
    }

    singleton_registry m_registry;
};

TEST_F(test_root_package, entry_test)
{
    auto& pkg = root::package::instance();
    ASSERT_EQ(pkg.get_type(), package_type::pt_static);

    ASSERT_TRUE(pkg.init_reflection());
    ASSERT_TRUE(pkg.override_reflection_types());
    ASSERT_TRUE(pkg.finilize_objects());
}