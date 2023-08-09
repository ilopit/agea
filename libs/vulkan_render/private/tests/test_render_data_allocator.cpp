
#include <gtest/gtest.h>

#include <utils/simple_allocator.h>
#include <vulkan_render/types/vulkan_render_data.h>

using namespace agea;
using namespace agea::render;

TEST(render_data_allocator_test, happy_pass)
{
    utils::combined_pool<vulkan_render_data> sp;

    auto o0 = sp.alloc(AID("id0"));
    auto o1 = sp.alloc(AID("id1"));
    auto o2 = sp.alloc(AID("id2"));

    ASSERT_EQ(o1->slot(), 1);
    ASSERT_EQ(o1->id(), AID("id1"));

    sp.release(o1);

    ASSERT_EQ(o1->slot(), uint32_t(-1));
    ASSERT_EQ(o1->id(), AID(""));

    auto o1_new = sp.alloc(AID("id1_new"));

    ASSERT_EQ(o1_new->slot(), 1);
    ASSERT_EQ(o1_new->id(), AID("id1_new"));

    ASSERT_EQ(sp.at(0)->id(), AID("id0"));
    ASSERT_EQ(sp.at(0)->slot(), 0);

    ASSERT_EQ(sp.at(1)->id(), AID("id1_new"));
    ASSERT_EQ(sp.at(1)->slot(), 1);

    ASSERT_EQ(sp.at(2)->id(), AID("id2"));
    ASSERT_EQ(sp.at(2)->slot(), 2);
}
