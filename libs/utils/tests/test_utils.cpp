#include "utils/clock.h"

#include <gtest/gtest.h>

using namespace agea::utils;

TEST(test_utils, test_clock)
{
    counter<10> ctr;

    for (uint64_t i = 1; i <= 10; ++i)
    {
        ctr.update(i);
    }

    ASSERT_NEAR(ctr.avg, 5.5, 0.0001);
    ASSERT_EQ(ctr.value, 10);
}