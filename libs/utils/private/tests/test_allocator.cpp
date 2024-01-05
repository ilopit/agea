#include "utils/stack_allocator.h"
#include "utils/gen_allocator.h"

#include <array>
#include <gtest/gtest.h>
#include <format>

using namespace agea::utils;

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

namespace
{
struct test_tracker : public gen_allocator::tracker
{
    struct ainfo
    {
        uint64_t id = 0;
        gen_allocator::memdescr block;
        uint32_t requested = 0;
    };

    struct rinfo
    {
        uint64_t id = 0;
        gen_allocator::memdescr block;
        void* requested = nullptr;
    };

    struct sinfo
    {
        uint64_t id = 0;
        gen_allocator::memdescr parent;
        gen_allocator::memdescr left;
        gen_allocator::memdescr right;
    };

    struct minfo
    {
        uint64_t id = 0;
        gen_allocator::memdescr left;
        gen_allocator::memdescr right;
        gen_allocator::memdescr result;
    };

    virtual void
    on_allocate(uint32_t id, const gen_allocator::memdescr& block, uint32_t requested) override
    {
        last_id = id;
        allocations.emplace_back(id, block, requested);
    }

    virtual void
    on_release(uint32_t id, const gen_allocator::memdescr& block, void* requested) override
    {
        last_id = id;
        releases.emplace_back(id, block, requested);
    }

    virtual void
    on_split(uint32_t id,
             const gen_allocator::memdescr& parent,
             const gen_allocator::memdescr& left,
             const gen_allocator::memdescr& right) override
    {
        last_id = id;
        splits.emplace_back(id, parent, left, right);
    }

    virtual void
    on_merge(uint32_t id,
             const gen_allocator::memdescr& left,
             const gen_allocator::memdescr& right,
             const gen_allocator::memdescr& result) override
    {
        last_id = id;
        merges.emplace_back(id, left, right, result);
    }

    ainfo*
    last_alloc()
    {
        if (allocations.empty())
        {
            return nullptr;
        }

        return allocations.back().id == last_id ? &allocations.back() : nullptr;
    }

    rinfo*
    last_release()
    {
        if (releases.empty())
        {
            return nullptr;
        }

        return releases.back().id == last_id ? &releases.back() : nullptr;
    }

    sinfo*
    last_split()
    {
        if (splits.empty())
        {
            return nullptr;
        }

        return splits.back().id == last_id ? &splits.back() : nullptr;
    }

    minfo*
    last_merge()
    {
        if (merges.empty())
        {
            return nullptr;
        }

        return merges.back().id == last_id ? &merges.back() : nullptr;
    }

    std::vector<ainfo> allocations;
    std::vector<rinfo> releases;
    std::vector<sinfo> splits;
    std::vector<minfo> merges;

    uint32_t last_id = 0;
};

constexpr uint64_t a_number = 4;
constexpr uint64_t a_size = 16;

class test_gen_allocator : public ::testing::Test
{
public:
    const uint32_t m_alloc_size = (gen_allocator::s_block_size + a_size) * a_number;

    void
    SetUp()
    {
        m_tt = std::make_unique<test_tracker>();
        m_allocator = std::make_unique<gen_allocator>(m_alloc_size, m_tt.get());
    }

    void
    verify()
    {
        for (auto& p : m_ids)
        {
            for (int j = 0; j < a_size - 2; ++j)
            {
                ASSERT_EQ(p.second[j], p.first);
            }
        }
    }

    bool
    alloc_with_id(uint64_t id)
    {
        auto ptr = (char*)m_allocator->alloc(a_size - 2);

        if (!ptr)
        {
            return false;
        }

        m_ids[id] = ptr;
        memset(ptr, id, a_size - 2);

        return true;
    }

    bool
    release(uint64_t id)
    {
        auto itr = m_ids.find(id);
        if (itr != m_ids.end())
        {
            m_allocator->release(itr->second);
            m_ids.erase(itr);
            return true;
        }

        return false;
    }

    std::map<uint64_t, char*> m_ids;
    std::unique_ptr<test_tracker> m_tt;
    std::unique_ptr<gen_allocator> m_allocator;
};
}  // namespace

TEST_F(test_gen_allocator, alloc_release_different_order)
{
    uint64_t id = 0;
    for (id = 0; id < a_number; ++id)
    {
        ASSERT_TRUE(alloc_with_id(id));
    }
    ASSERT_FALSE(alloc_with_id(++id));

    verify();

    for (int i = a_number - 1; i >= 0; --i)
    {
        release(i);
    }
    verify();

    id += 5;
    // *****************************
    for (int i = 0; i < a_number; ++i)
    {
        ASSERT_TRUE(alloc_with_id(id));
    }
    ASSERT_FALSE(alloc_with_id(id));

    verify();

    for (int i = 0; i < a_number; ++i)
    {
        release(i);
    }

    verify();
}

TEST_F(test_gen_allocator, simple_alloc)
{
    auto data1 = m_allocator->alloc(64);
    ASSERT_TRUE(data1);

    ASSERT_EQ(m_tt->allocations.size(), 1U);

    auto alloc = m_tt->last_alloc();
    ASSERT_TRUE(alloc);

    ASSERT_EQ(alloc->block.block_id, 0);
    ASSERT_EQ(alloc->block.size, 64 + gen_allocator::s_block_size);

    ASSERT_EQ(m_tt->splits.size(), 1U);

    auto split = m_tt->last_split();
    ASSERT_TRUE(split);

    ASSERT_EQ(split->parent.block_id, 0);
    ASSERT_EQ(split->parent.size, m_allocator->m_alloc_size);

    ASSERT_EQ(split->left.block_id, 0);
    ASSERT_EQ(split->left.size, 64 + gen_allocator::s_block_size);

    ASSERT_EQ(split->right.block_id, 1);
    ASSERT_EQ(split->right.size, m_allocator->m_alloc_size - 64 - gen_allocator::s_block_size);

    auto data2 = m_allocator->alloc(m_allocator->m_alloc_size);
    ASSERT_FALSE(data2);

    ASSERT_EQ(m_tt->splits.size(), 1U);
    ASSERT_EQ(m_tt->allocations.size(), 1U);

    m_allocator->release(data1);

    ASSERT_EQ(m_tt->releases.size(), 1U);

    auto merge = m_tt->last_merge();

    ASSERT_EQ(merge->left.block_id, 0);
    ASSERT_EQ(merge->left.size, 64 + gen_allocator::s_block_size);
    ASSERT_EQ(merge->right.block_id, 1);
    ASSERT_EQ(merge->right.size, m_allocator->m_alloc_size - gen_allocator::s_block_size - 64);
    ASSERT_EQ(merge->result.block_id, 0);
    ASSERT_EQ(merge->result.size, m_allocator->m_alloc_size);
}

TEST_F(test_gen_allocator, merging_left)
{
    auto data_1 = m_allocator->alloc(a_size);
    ASSERT_TRUE(data_1);
    auto data_2 = m_allocator->alloc(a_size);
    ASSERT_TRUE(data_2);
    auto data_3 = m_allocator->alloc(a_size);
    ASSERT_TRUE(data_3);
    auto data_4 = m_allocator->alloc(a_size);
    ASSERT_TRUE(data_4);

    m_allocator->release(data_2);

    auto release = m_tt->last_release();
    ASSERT_TRUE(release);

    auto merge = m_tt->last_merge();
    ASSERT_FALSE(merge);

    m_allocator->release(data_1);

    release = m_tt->last_release();
    ASSERT_TRUE(release);

    merge = m_tt->last_merge();
    ASSERT_TRUE(merge);

    ASSERT_EQ(merge->left.block_id, 0);
    ASSERT_EQ(merge->right.block_id, 1);
}

TEST_F(test_gen_allocator, merging_right)
{
    auto data_1 = m_allocator->alloc(a_size);
    ASSERT_TRUE(data_1);
    auto data_2 = m_allocator->alloc(a_size);
    ASSERT_TRUE(data_2);
    auto data_3 = m_allocator->alloc(a_size);
    ASSERT_TRUE(data_3);
    auto data_4 = m_allocator->alloc(a_size);
    ASSERT_TRUE(data_4);

    m_allocator->release(data_2);

    auto release = m_tt->last_release();
    ASSERT_TRUE(release);

    auto merge = m_tt->last_merge();
    ASSERT_FALSE(merge);

    m_allocator->release(data_3);

    release = m_tt->last_release();
    ASSERT_TRUE(release);

    merge = m_tt->last_merge();
    ASSERT_TRUE(merge);

    ASSERT_EQ(merge->left.block_id, 1);
    ASSERT_EQ(merge->right.block_id, 2);
}
