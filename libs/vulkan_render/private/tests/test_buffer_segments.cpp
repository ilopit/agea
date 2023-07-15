
#include <gtest/gtest.h>

#include "vulkan_render/utils/segments.h"

using namespace agea::render;

TEST(SimpleTest, happy)
{
    buffer_layout<int*> bb;

    bb.add(AID("1"), 1, 2);
    bb.add(AID("2"), 1, 3);
    bb.add(AID("3"), 1, 2);
    bb.add(AID("4"), 1, 1);
    bb.add(AID("5"), 1, 2);

    auto ptr = bb.find(AID("4"));

    auto size_a = bb.size();
    auto size_n = bb.calc_new_size();

    ptr->alloc_id();
    ptr->alloc_id();
    ptr->alloc_id();

    size_a = bb.size();
    size_n = bb.calc_new_size();

    for (uint64_t i = 0; i < bb.get_segments_size(); ++i)
    {
        bb.update_segment(i);
    }

    size_a = bb.size();
    size_n = bb.calc_new_size();

    int i = 2;
}