#include "utils/clock.h"
#include "utils/dynamic_object.h"

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

TEST(test_utils, test_dynamic_object)
{
    {
        dynamic_object_layout_sequence_builder builder;

        builder.add_field(AID("f1"), agea_type::t_f, 4);
        builder.add_field(AID("v4"), agea_type::t_vec4, 16);
        builder.add_field(AID("f2"), agea_type::t_f, 4);

        auto layout = builder.get_layout();

        ASSERT_EQ(layout->get_size(), 36);
        ASSERT_EQ(layout->get_fields().size(), 3);

        {
            ASSERT_EQ(layout->get_fields()[0].offset, 0);
            ASSERT_EQ(layout->get_fields()[0].size, 4);
            ASSERT_EQ(layout->get_fields()[0].id, AID("f1"));
        }
        {
            ASSERT_EQ(layout->get_fields()[1].offset, 16);
            ASSERT_EQ(layout->get_fields()[1].size, 16);
            ASSERT_EQ(layout->get_fields()[1].id, AID("v4"));
        }
        {
            ASSERT_EQ(layout->get_fields()[2].offset, 32);
            ASSERT_EQ(layout->get_fields()[2].size, 4);
            ASSERT_EQ(layout->get_fields()[2].id, AID("f2"));
        }
    }
    {
        dynamic_object_layout_sequence_builder builder;

        builder.add_field(AID("v31"), agea_type::t_vec3, 16);
        builder.add_field(AID("v32"), agea_type::t_vec3, 16);
        builder.add_field(AID("v33"), agea_type::t_vec3, 16);
        builder.add_field(AID("f1"), agea_type::t_f, 4);

        auto layout = builder.get_layout();

        ASSERT_EQ(layout->get_size(), 48);
        ASSERT_EQ(layout->get_fields().size(), 4);

        {
            ASSERT_EQ(layout->get_fields()[0].offset, 0);
            ASSERT_EQ(layout->get_fields()[0].size, 12);
            ASSERT_EQ(layout->get_fields()[0].id, AID("v31"));
        }
        {
            ASSERT_EQ(layout->get_fields()[1].offset, 16);
            ASSERT_EQ(layout->get_fields()[1].size, 12);
            ASSERT_EQ(layout->get_fields()[1].id, AID("v32"));
        }
        {
            ASSERT_EQ(layout->get_fields()[2].offset, 32);
            ASSERT_EQ(layout->get_fields()[2].size, 12);
            ASSERT_EQ(layout->get_fields()[2].id, AID("v33"));
        }
        {
            ASSERT_EQ(layout->get_fields()[3].offset, 44);
            ASSERT_EQ(layout->get_fields()[3].size, 4);
            ASSERT_EQ(layout->get_fields()[3].id, AID("f1"));
        }
    }
}