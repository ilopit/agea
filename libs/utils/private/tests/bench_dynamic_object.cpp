#include <gtest/gtest.h>

#include "utils/dynamic_object.h"
#include "utils/dynamic_object_v2.h"
#include "utils/dynamic_object_builder.h"

#include <chrono>
#include <iostream>

namespace agea::utils
{

// Type IDs for testing
constexpr uint32_t TYPE_FLOAT = 1;
constexpr uint32_t TYPE_INT32 = 2;
constexpr uint32_t TYPE_VEC3 = 3;

//=============================================================================
// Benchmark utilities
//=============================================================================
class scoped_timer
{
public:
    explicit scoped_timer(const char* name)
        : m_name(name)
        , m_start(std::chrono::high_resolution_clock::now())
    {}

    ~scoped_timer()
    {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);
        std::cout << m_name << ": " << duration.count() << " us" << std::endl;
    }

private:
    const char* m_name;
    std::chrono::high_resolution_clock::time_point m_start;
};

//=============================================================================
// V2 Implementation Tests
//=============================================================================
TEST(dynamic_object_v2, layout_builder)
{
    auto layout_idx = dynobj_layout_builder_v2(AID("test_struct"))
        .add_field(AID("x"), TYPE_FLOAT, sizeof(float), 4)
        .add_field(AID("y"), TYPE_FLOAT, sizeof(float), 4)
        .add_field(AID("z"), TYPE_FLOAT, sizeof(float), 4)
        .add_field(AID("count"), TYPE_INT32, sizeof(int32_t), 4)
        .build_and_register();

    ASSERT_NE(layout_idx, 0);

    auto* layout = layout_registry_v2::instance().get(layout_idx);
    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(layout->field_count(), 4);
    EXPECT_GE(layout->object_size(), 16);
}

TEST(dynamic_object_v2, basic_read_write)
{
    auto layout_idx = dynobj_layout_builder_v2(AID("vec3"))
        .add_field(AID("x"), TYPE_FLOAT, sizeof(float), 4)
        .add_field(AID("y"), TYPE_FLOAT, sizeof(float), 4)
        .add_field(AID("z"), TYPE_FLOAT, sizeof(float), 4)
        .build_and_register();

    dynobj_v2 obj(layout_idx);
    ASSERT_FALSE(obj.empty());

    auto view = obj.view();

    // Write using unchecked access
    view.get<float>(0) = 1.0f;
    view.get<float>(1) = 2.0f;
    view.get<float>(2) = 3.0f;

    // Read back
    EXPECT_FLOAT_EQ(view.get<float>(0), 1.0f);
    EXPECT_FLOAT_EQ(view.get<float>(1), 2.0f);
    EXPECT_FLOAT_EQ(view.get<float>(2), 3.0f);
}

TEST(dynamic_object_v2, checked_access)
{
    auto layout_idx = dynobj_layout_builder_v2(AID("test"))
        .add_field(AID("value"), TYPE_FLOAT, sizeof(float), 4)
        .build_and_register();

    dynobj_v2 obj(layout_idx);
    auto view = obj.view();

    // Valid set
    EXPECT_TRUE(view.try_set<float>(0, 42.0f));

    // Valid get
    float val = 0.0f;
    EXPECT_TRUE(view.try_get<float>(0, val));
    EXPECT_FLOAT_EQ(val, 42.0f);

    // Invalid field index
    EXPECT_FALSE(view.try_get<float>(99, val));

    // Wrong size type
    EXPECT_FALSE(view.try_get<double>(0, *reinterpret_cast<double*>(&val)));
}

TEST(dynamic_object_v2, array_access)
{
    auto layout_idx = dynobj_layout_builder_v2(AID("array_test"))
        .add_field(AID("values"), TYPE_FLOAT, sizeof(float), 4, 4) // array of 4 floats
        .build_and_register();

    dynobj_v2 obj(layout_idx);
    auto view = obj.view();

    // Write array elements
    for (uint32_t i = 0; i < 4; ++i)
    {
        view.get_array<float>(0, i) = static_cast<float>(i * 10);
    }

    // Read back
    for (uint32_t i = 0; i < 4; ++i)
    {
        EXPECT_FLOAT_EQ(view.get_array<float>(0, i), static_cast<float>(i * 10));
    }
}

TEST(dynamic_object_v2, inline_storage)
{
    auto layout_idx = dynobj_layout_builder_v2(AID("small"))
        .add_field(AID("a"), TYPE_FLOAT, sizeof(float), 4)
        .add_field(AID("b"), TYPE_FLOAT, sizeof(float), 4)
        .build_and_register();

    dynobj_v2 obj(layout_idx);
    EXPECT_TRUE(obj.is_inline());
    EXPECT_LE(obj.size(), 64);
}

TEST(dynamic_object_v2, heap_storage)
{
    // Create a layout larger than inline storage
    auto builder = dynobj_layout_builder_v2(AID("large"));
    for (int i = 0; i < 20; ++i)
    {
        builder.add_field(AID(std::format("field_{}", i).c_str()), TYPE_FLOAT, sizeof(float), 4);
    }
    auto layout_idx = builder.build_and_register();

    dynobj_v2 obj(layout_idx);
    EXPECT_FALSE(obj.is_inline());
    EXPECT_GT(obj.size(), 64);
}

TEST(dynamic_object_v2, copy_semantics)
{
    auto layout_idx = dynobj_layout_builder_v2(AID("copy_test"))
        .add_field(AID("value"), TYPE_INT32, sizeof(int32_t), 4)
        .build_and_register();

    dynobj_v2 obj1(layout_idx);
    obj1.view().get<int32_t>(0) = 12345;

    // Copy construct
    dynobj_v2 obj2(obj1);
    EXPECT_EQ(obj2.view().get<int32_t>(0), 12345);

    // Modify original shouldn't affect copy
    obj1.view().get<int32_t>(0) = 99999;
    EXPECT_EQ(obj2.view().get<int32_t>(0), 12345);
}

TEST(dynamic_object_v2, move_semantics)
{
    auto layout_idx = dynobj_layout_builder_v2(AID("move_test"))
        .add_field(AID("value"), TYPE_INT32, sizeof(int32_t), 4)
        .build_and_register();

    dynobj_v2 obj1(layout_idx);
    obj1.view().get<int32_t>(0) = 12345;

    // Move construct
    dynobj_v2 obj2(std::move(obj1));
    EXPECT_EQ(obj2.view().get<int32_t>(0), 12345);
    EXPECT_TRUE(obj1.empty());
}

//=============================================================================
// Benchmarks (compare V1 vs V2)
//=============================================================================
TEST(dynamic_object_benchmark, DISABLED_v2_write_performance)
{
    auto layout_idx = dynobj_layout_builder_v2(AID("bench"))
        .add_field(AID("x"), TYPE_FLOAT, sizeof(float), 4)
        .add_field(AID("y"), TYPE_FLOAT, sizeof(float), 4)
        .add_field(AID("z"), TYPE_FLOAT, sizeof(float), 4)
        .build_and_register();

    dynobj_v2 obj(layout_idx);
    auto view = obj.view();

    constexpr int ITERATIONS = 1000000;

    {
        scoped_timer timer("V2 unchecked write (1M iterations)");
        for (int i = 0; i < ITERATIONS; ++i)
        {
            view.get<float>(0) = static_cast<float>(i);
            view.get<float>(1) = static_cast<float>(i + 1);
            view.get<float>(2) = static_cast<float>(i + 2);
        }
    }

    {
        scoped_timer timer("V2 checked write (1M iterations)");
        for (int i = 0; i < ITERATIONS; ++i)
        {
            view.try_set<float>(0, static_cast<float>(i));
            view.try_set<float>(1, static_cast<float>(i + 1));
            view.try_set<float>(2, static_cast<float>(i + 2));
        }
    }
}

TEST(dynamic_object_benchmark, DISABLED_v2_read_performance)
{
    auto layout_idx = dynobj_layout_builder_v2(AID("bench"))
        .add_field(AID("x"), TYPE_FLOAT, sizeof(float), 4)
        .add_field(AID("y"), TYPE_FLOAT, sizeof(float), 4)
        .add_field(AID("z"), TYPE_FLOAT, sizeof(float), 4)
        .build_and_register();

    dynobj_v2 obj(layout_idx);
    auto view = obj.view();

    view.get<float>(0) = 1.0f;
    view.get<float>(1) = 2.0f;
    view.get<float>(2) = 3.0f;

    constexpr int ITERATIONS = 1000000;
    volatile float sum = 0.0f;

    {
        scoped_timer timer("V2 unchecked read (1M iterations)");
        for (int i = 0; i < ITERATIONS; ++i)
        {
            sum += view.get<float>(0);
            sum += view.get<float>(1);
            sum += view.get<float>(2);
        }
    }

    float val = 0.0f;
    {
        scoped_timer timer("V2 checked read (1M iterations)");
        for (int i = 0; i < ITERATIONS; ++i)
        {
            view.try_get<float>(0, val);
            sum += val;
            view.try_get<float>(1, val);
            sum += val;
            view.try_get<float>(2, val);
            sum += val;
        }
    }

    // Prevent optimization
    EXPECT_GT(sum, 0.0f);
}

TEST(dynamic_object_benchmark, DISABLED_object_creation)
{
    auto layout_idx = dynobj_layout_builder_v2(AID("bench"))
        .add_field(AID("x"), TYPE_FLOAT, sizeof(float), 4)
        .add_field(AID("y"), TYPE_FLOAT, sizeof(float), 4)
        .add_field(AID("z"), TYPE_FLOAT, sizeof(float), 4)
        .build_and_register();

    constexpr int ITERATIONS = 100000;

    {
        scoped_timer timer("V2 object creation inline (100K iterations)");
        for (int i = 0; i < ITERATIONS; ++i)
        {
            dynobj_v2 obj(layout_idx);
            (void)obj;
        }
    }
}

} // namespace agea::utils
