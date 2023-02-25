#include "utils/clock.h"
#include "utils/dynamic_object.h"
#include "utils/dynamic_object_builder.h"

#include <gtest/gtest.h>

using namespace agea::utils;

struct test_type
{
    enum class id
    {
        id_nan = 0,
        id_float,
        id_uint32,
        id_vec3,
        id_vec4
    };

    static constexpr uint32_t
    size(test_type::id t)
    {
        switch (t)
        {
        case test_type::id::id_float:
            return 4;
        case test_type::id::id_uint32:
            return 4;
        case test_type::id::id_vec3:
            return 12;
        case test_type::id::id_vec4:
            return 16;
        case test_type::id::id_nan:
        default:
            return 0;
        }
    }

    template <typename T>
    static constexpr id
    decode(const T&)
    {
        if constexpr (std::is_same<T, float>::value)
        {
            return id::id_float;
        }
        else if constexpr (std::is_same<T, uint32_t>::value)
        {
            return id::id_uint32;
        }
        else if constexpr (std::is_same<T, std::array<float, 3>>::value)
        {
            return id::id_vec3;
        }

        return id::id_nan;
    }

    template <typename T>
    static constexpr uint32_t
    decode_as_int(const T& v)
    {
        return (uint32_t)decode(v);
    }
};

using test_dynobj_builder = dynamic_object_layout_sequence_builder<test_type>;

TEST(test_utils, DISABLE_test_clock)
{
    counter<10> ctr;

    for (uint64_t i = 1; i <= 10; ++i)
    {
        ctr.update(i);
    }

    ASSERT_NEAR(ctr.avg, 5.5, 0.0001);
    ASSERT_EQ(ctr.value, 10);
}

TEST(test_utils, DISABLE_test_dynamic_object)
{
    {
        test_dynobj_builder builder;

        builder.add_field(AID("f1"), test_type::id::id_float, 4);
        builder.add_field(AID("v4"), test_type::id::id_vec4, 16);
        builder.add_field(AID("f2"), test_type::id::id_float, 4);

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
        test_dynobj_builder builder;

        builder.add_field(AID("v31"), test_type::id::id_vec3, 16);
        builder.add_field(AID("v32"), test_type::id::id_vec3, 16);
        builder.add_field(AID("v33"), test_type::id::id_vec3, 16);
        builder.add_field(AID("f1"), test_type::id::id_float, 4);

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

TEST(test_utils, test_dynamic_object_write)
{
    test_dynobj_builder builder;

    builder.add_field(AID("i1"), test_type::id::id_uint32, 1);
    builder.add_field(AID("i2"), test_type::id::id_uint32, 8);
    builder.add_field(AID("i3"), test_type::id::id_uint32, 16);
    builder.add_field(AID("v3"), test_type::id::id_vec3, 3);

    auto layout = builder.get_layout();

    auto obj = layout->get_empty_obj();

    const uint32_t v1 = 0x12345678;
    const uint32_t v2 = 0xFEFEFEFE;
    const uint32_t v3 = 0xABCDDCBA;
    const std::array<float, 3> vec3{
        0.000001f,
        1.f,
        1000000.f,
    };

    {
        auto res = obj.write<test_type>(0, v1, v2, v3, vec3);
        ASSERT_TRUE(res);
    }

    {
        uint32_t rv1 = 0, rv2 = 0, rv3 = 0;
        std::array<float, 3> rvec3;

        auto res = obj.read<test_type>(0, rv1, rv2, rv3, rvec3);
        ASSERT_TRUE(res);
        ASSERT_EQ(v1, rv1);
        ASSERT_EQ(v2, rv2);
        ASSERT_EQ(v3, rv3);
        ASSERT_EQ(vec3, rvec3);
    }
}