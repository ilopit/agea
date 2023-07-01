#include "utils/clock.h"
#include "utils/dynamic_object.h"
#include "utils/dynamic_object_builder.h"
#include "utils/math_utils.h"
#include "utils/agea_log.h"

#include <gtest/gtest.h>

using namespace agea::utils;

namespace
{
auto r = []()
{
    agea::utils::setup_logger();
    return true;
}();
}

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

    static constexpr uint64_t
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

TEST(test_utils, test_dynamic_object_layout)
{
    {
        test_dynobj_builder builder;

        builder.add_field(AID("f1"), test_type::id::id_float, 4);
        builder.add_field(AID("v4"), test_type::id::id_vec4, 16);
        builder.add_field(AID("f2"), test_type::id::id_float, 4);

        auto layout = builder.get_layout();

        ASSERT_EQ(layout->get_object_size(), 36);
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
        test_dynobj_builder sub_builder;
        sub_builder.add_field(AID("u0"), test_type::id::id_uint32, 16);
        sub_builder.add_field(AID("u1"), test_type::id::id_uint32, 16);
        sub_builder.add_field(AID("u2"), test_type::id::id_uint32, 16);

        test_dynobj_builder builder;
        builder.add_array(AID("array"), test_type::id::id_float, 1, 5, 8);
        builder.add_field(AID("v31"), test_type::id::id_vec3, 16);
        builder.add_field(AID("v32"), test_type::id::id_vec3, 16);
        builder.add_field(AID("v33"), test_type::id::id_vec3, 16);
        builder.add_field(AID("sub_obj"), sub_builder.get_layout(), 1);
        builder.add_field(AID("f1"), test_type::id::id_float, 4);

        auto layout = builder.get_layout();

        ASSERT_EQ(layout->get_object_size(), 132);
        ASSERT_EQ(layout->get_fields().size(), 6);

        {
            ASSERT_EQ(layout->get_fields()[0].offset, 0);
            ASSERT_EQ(layout->get_fields()[0].size, 36);
            ASSERT_EQ(layout->get_fields()[0].id, AID("array"));
            ASSERT_EQ(layout->get_fields()[0].index, 0);
        }
        {
            ASSERT_EQ(layout->get_fields()[1].offset, 48);
            ASSERT_EQ(layout->get_fields()[1].size, 12);
            ASSERT_EQ(layout->get_fields()[1].id, AID("v31"));
            ASSERT_EQ(layout->get_fields()[1].index, 1);
        }
        {
            ASSERT_EQ(layout->get_fields()[2].offset, 64);
            ASSERT_EQ(layout->get_fields()[2].size, 12);
            ASSERT_EQ(layout->get_fields()[2].id, AID("v32"));
            ASSERT_EQ(layout->get_fields()[2].index, 2);
        }
        {
            ASSERT_EQ(layout->get_fields()[3].offset, 80);
            ASSERT_EQ(layout->get_fields()[3].size, 12);
            ASSERT_EQ(layout->get_fields()[3].id, AID("v33"));
            ASSERT_EQ(layout->get_fields()[3].index, 3);
        }
        {
            ASSERT_EQ(layout->get_fields()[4].offset, 92);
            ASSERT_EQ(layout->get_fields()[4].size, 36);
            ASSERT_EQ(layout->get_fields()[4].id, AID("sub_obj"));
            ASSERT_EQ(layout->get_fields()[4].index, 4);
        }
        {
            ASSERT_EQ(layout->get_fields()[5].offset, 128);
            ASSERT_EQ(layout->get_fields()[5].size, 4);
            ASSERT_EQ(layout->get_fields()[5].id, AID("f1"));
            ASSERT_EQ(layout->get_fields()[5].index, 5);
        }
    }
}

TEST(test_utils, test_round_to_next)
{
    ASSERT_EQ(math_utils::align_as(10, 3), 12);
    ASSERT_EQ(math_utils::align_as(2, 3), 3);
}

TEST(test_utils, test_dynamic_object_array)
{
    auto l = test_dynobj_builder()
                 .add_field(AID("float_array"), test_type::id::id_float)
                 .add_array(AID("float_array"), test_type::id::id_float, 4, 5, 8)
                 .add_field(AID("float_array"), test_type::id::id_float)
                 .finalize();

    auto obj = l->make_obj();
    auto v = l->make_view<test_type>();

    ASSERT_EQ(obj.size(), 4 + 8 * 4 + 4 + 4);
}

TEST(test_utils, test_dynamic_object_array_2)
{
    auto l0 = test_dynobj_builder()
                  .add_field(AID("u0"), test_type::id::id_uint32, 16)
                  .add_field(AID("u1"), test_type::id::id_uint32, 16)
                  .add_field(AID("u2"), test_type::id::id_uint32, 16)
                  .add_array(AID("u3"), test_type::id::id_uint32, 16, 5, 1)
                  .finalize();

    auto l1 = test_dynobj_builder()
                  .add_field(AID("sf0"), test_type::id::id_float, 16)
                  .add_field(AID("su1"), l0, 16)
                  .finalize();

    dynobj dyn_object(l1);

    auto acs = dyn_object.root<test_type>();

    auto f1 = acs.subobj(1);

    uint32_t v0 = 0, v1 = 0, v2 = 0;
    ASSERT_TRUE(f1.write_array(3, 0, 1U, 2U, 3U));

    v0 = 0, v1 = 0, v2 = 0;
    ASSERT_TRUE(f1.read_array(3, 0, v0, v1, v2));

    ASSERT_EQ(v0, 1);
    ASSERT_EQ(v1, 2);
    ASSERT_EQ(v2, 3);
}

struct simple_obj
{
    alignas(16) float f0 = 1.0f;
    alignas(16) uint32_t f1 = 2U;
    alignas(16) std::array<uint32_t, 4> f2 = {3, 4, 5, 6};
};

TEST(test_utils, test_dynview)
{
    auto l = test_dynobj_builder()
                 .add_field(AID("f0"), test_type::id::id_float, 16)
                 .add_field(AID("f1"), test_type::id::id_uint32, 16)
                 .add_array(AID("f2"), test_type::id::id_uint32, 16, 4, 1)
                 .finalize();

    simple_obj static_obj;

    auto v = l->make_view<test_type>(&static_obj);
    ASSERT_TRUE(v.is_object());

    float f0 = 0.f;
    uint32_t f1 = 0U;
    std::array<uint32_t, 4> f2 = {0};

    ASSERT_TRUE(v.read(0, f0, f1));
    ASSERT_TRUE(v.read_array(2, 0, f2[0], f2[1], f2[2], f2[3]));

    ASSERT_EQ(f0, static_obj.f0);
    ASSERT_EQ(f1, static_obj.f1);
    ASSERT_EQ(f2, static_obj.f2);

    ASSERT_TRUE(v.write(0, 2.f, 4U));
    ASSERT_TRUE(v.write_array(2, 0, 6U, 8U, 10U, 12U));

    std::array<uint32_t, 4> new_f2{6, 8, 10, 12};

    ASSERT_EQ(static_obj.f0, 2.f);
    ASSERT_EQ(static_obj.f1, 4);
    ASSERT_EQ(static_obj.f2, new_f2);
}

namespace
{
struct array_obj
{
    simple_obj a0[5];
};
}  // namespace

TEST(test_utils, test_dynview_array)
{
    auto item = test_dynobj_builder()
                    .add_field(AID("f0"), test_type::id::id_float, 16)
                    .add_field(AID("f1"), test_type::id::id_uint32, 16)
                    .add_array(AID("f2"), test_type::id::id_uint32, 16, 4, 1)
                    .finalize();

    auto array = test_dynobj_builder().add_array(AID("a0"), item, 1, 5, 16).finalize();

    array_obj static_obj;

    std::array<uint32_t, 4> f2{6, 8, 10, 12};
    float f0 = 0.f;
    uint32_t f1 = 0U;

    static_obj.a0[3].f0 = 2.f;
    static_obj.a0[3].f1 = 4;
    static_obj.a0[3].f2 = f2;

    auto v = array->make_view<test_type>(&static_obj);

    ASSERT_TRUE(v.is_object());

    auto v3 = v.subobj(0, 3);

    ASSERT_TRUE(v3.read(0, f0, f1));
    ASSERT_TRUE(v3.read_array(2, 0, f2[0], f2[1], f2[2], f2[3]));

    ASSERT_EQ(static_obj.a0[3].f0, f0);
    ASSERT_EQ(static_obj.a0[3].f1, f1);
    ASSERT_EQ(static_obj.a0[3].f2, f2);

    auto v1 = v.subobj(0, 1);

    ASSERT_TRUE(v1.write(0, f0, f1));
    ASSERT_TRUE(v1.write_array(2, 0, f2[0], f2[1], f2[2], f2[3]));

    f2.fill(0);
    f0 = 0.f;
    f1 = 0U;

    ASSERT_TRUE(v1.read(0, f0, f1));
    ASSERT_TRUE(v1.read_array(2, 0, f2[0], f2[1], f2[2], f2[3]));

    ASSERT_EQ(static_obj.a0[1].f0, f0);
    ASSERT_EQ(static_obj.a0[1].f1, f1);
    ASSERT_EQ(static_obj.a0[1].f2, f2);
}

namespace
{
struct nested_obj
{
    alignas(16) float n0 = 9.0;
    alignas(16) simple_obj n1;
};

struct parent_obj
{
    alignas(16) uint32_t p0 = 10;
    alignas(16) uint32_t p1 = 12;
    alignas(16) nested_obj p2;
};

}  // namespace

TEST(test_utils, test_dynview_nested)
{
    auto so = test_dynobj_builder()
                  .add_field(AID("f0"), test_type::id::id_float, 16)
                  .add_field(AID("f1"), test_type::id::id_uint32, 16)
                  .add_array(AID("f2"), test_type::id::id_uint32, 16, 4, 1)
                  .finalize();

    auto no = test_dynobj_builder()
                  .add_field(AID("n0"), test_type::id::id_float, 16)
                  .add_field(AID("n1"), so, 16)
                  .finalize();

    auto po = test_dynobj_builder()
                  .add_field(AID("p0"), test_type::id::id_uint32, 16)
                  .add_field(AID("p1"), test_type::id::id_uint32, 16)
                  .add_field(AID("p2"), no, 16)
                  .finalize();

    parent_obj static_obj;

    auto v = po->make_view<test_type>(&static_obj);

    auto v_simple = v.subobj(2).subobj(1);

    float f0 = 0.f;
    uint32_t f1 = 0U;
    std::array<uint32_t, 4> f2 = {0};

    ASSERT_TRUE(v_simple.read(0, f0, f1));
    ASSERT_TRUE(v_simple.read_array(2, 0, f2[0], f2[1], f2[2], f2[3]));

    ASSERT_EQ(f0, static_obj.p2.n1.f0);
    ASSERT_EQ(f1, static_obj.p2.n1.f1);
    ASSERT_EQ(f2, static_obj.p2.n1.f2);

    ASSERT_TRUE(v_simple.write(0, 2.f, 4U));
    ASSERT_TRUE(v_simple.write_array(2, 0, 6U, 8U, 10U, 12U));

    std::array<uint32_t, 4> new_f2{6, 8, 10, 12};

    ASSERT_EQ(static_obj.p2.n1.f0, 2.f);
    ASSERT_EQ(static_obj.p2.n1.f1, 4);
    ASSERT_EQ(static_obj.p2.n1.f2, new_f2);
}