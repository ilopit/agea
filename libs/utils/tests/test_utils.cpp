#include <utils/weird_singletone.h>

#include <gtest/gtest.h>

namespace
{
struct to_test : agea::selfcleanable_singleton<float>
{
    static float*
    test_get()
    {
        return s_obj;
    }
};
}  // namespace

TEST(test_utils, singleton)
{
    ASSERT_FALSE(to_test::test_get());
    {
        std::unique_ptr<agea::base_deleter> ptr = to_test::create();

        ASSERT_TRUE(to_test::test_get());
        *to_test::test_get() = 5.f;

        ASSERT_EQ(*to_test::test_get(), 5.f);
    }
    ASSERT_FALSE(to_test::test_get());
}