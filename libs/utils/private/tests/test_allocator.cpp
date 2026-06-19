#include "utils/stack_allocator.h"

#include <array>
#include <gtest/gtest.h>
#include <kryga_port/format.h>

using namespace kryga::utils;

TEST(test_allocator, simple)
{
    stack_allocator alctr(256);

    char* arr[4] = {};
    {
        uint32_t size = 3;
        arr[0] = (char*)alctr.alloc_item(size);
        memset(arr[0], 0, size);
        ASSERT_EQ(alctr.used(), 32);
    }

    {
        uint32_t size = 17;
        arr[1] = (char*)alctr.alloc_item(size);
        memset(arr[1], 0, size);
        ASSERT_EQ(alctr.used(), 80);
    }
    {
        uint32_t size = 33;
        arr[2] = (char*)alctr.alloc_item(size);
        memset(arr[2], 0, size);
        ASSERT_EQ(alctr.used(), 144);
    }
    {
        uint32_t size = 43;
        arr[3] = (char*)alctr.alloc_item(size);
        memset(arr[3], 0, size);
        ASSERT_EQ(alctr.used(), 208);
    }
    {
        uint32_t size = 43;
        auto ptr = alctr.alloc_item(size);
        ASSERT_FALSE(nullptr);
    }

    alctr.dealloc(arr[1]);
    alctr.dealloc(arr[2]);
    alctr.dealloc(arr[3]);

    {
        uint32_t size = 17;
        arr[0] = (char*)alctr.alloc_item(size);
        memset(arr[1], 'a', size);
        ASSERT_EQ(alctr.used(), 80);
    }

    auto data = (char*)alctr.sysmem();

    for (int i = 0; i < 256; ++i)
    {
        std::cout << std::hex << int(*(data + i));
    }
}

namespace
{

const uint64_t allocations_size = 4000000;

std::vector<uint32_t> allocators = []()
{
    std::vector<uint32_t> ss;
    for (int i = 0; i < allocations_size; ++i)
    {
        ss.push_back((rand() + 1) % (128 * 1024));
    }

    return ss;
}();

std::vector<char*> holders(allocations_size);

}  // namespace

TEST(test_allocator, performance)
{
    int64_t limit = 1024 * 1024 * 512;
    int64_t total = 0;
    int j = 0;

    for (int i = 0; i < allocations_size; ++i)
    {
        if (total < limit)
        {
            holders[i] = new char[allocators[i]];
            total += allocators[i];
        }
        else
        {
            delete holders[j];
            total -= allocators[j];
            ++j;
        }
    }
}

TEST(test_allocator, performance_2)
{
    int64_t limit = 1024 * 1024 * 512;
    stack_allocator alctr(limit);
    int64_t total = 0;
    int j = 0;

    for (int i = 0; i < allocations_size; ++i)
    {
        if (total < limit)
        {
            holders[i] = (char*)alctr.alloc_item(allocators[i]);
            total += allocators[i];
        }
        else
        {
            alctr.dealloc(holders[j]);
            total -= allocators[j];
            ++j;
        }
    }
}
