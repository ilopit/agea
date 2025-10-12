
#include "core/object_constructor.h"

#include <core/package_manager.h>
#include <core/level.h>
#include <core/level_manager.h>

#include <utils/agea_log.h>
#include <utils/file_utils.h>

#include <gtest/gtest.h>

#include "base_test.h"

using namespace agea;

struct test_load_level : public base_test
{
    void
    SetUp()
    {
        base_test::SetUp();
    }

    void
    TearDown()
    {
        base_test::TearDown();
    }
};

TEST_F(test_load_level, load_level)
{
    // core::level_manager lm;
    // auto id = AID("light_sandbox");
    // auto lvl = lm.load_level(id, &global, &local);
}
