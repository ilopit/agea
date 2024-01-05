#include "utils/gen_allocator.h"

#include <stdlib.h>
#include <format>
#include <iostream>

namespace agea::utils
{

namespace
{
const uint16_t kHEADER = 0XFDA;
}

gen_allocator::memdescr*
make_header(void* ptr, int64_t offset)
{
    return (gen_allocator::memdescr*)((uint8_t*)ptr + offset);
}

void*
offset(void* ptr, int64_t offset)
{
    return ((uint8_t*)ptr + offset);
}

gen_allocator::gen_allocator(uint64_t size, tracker* t)
    : m_alloc_size(math_utils::align_as_pow2(size, 16) + gen_allocator::s_block_size)
    , m_data((uint8_t*)malloc(m_alloc_size))
    , m_all_head((memdescr*)m_data)
    , m_free_head(m_all_head)
    , m_tracker(t)
{
    m_all_head->next = m_all_head;
    m_all_head->prev = m_all_head;
    m_all_head->free_next = m_all_head;
    m_all_head->free_prev = m_all_head;
    m_all_head->free = true;
    m_all_head->block_id = m_block_id++;
    m_all_head->size = m_alloc_size;
    m_all_head->header = kHEADER;

    m_free_head->next = m_free_head;
}

void*
gen_allocator::alloc(uint32_t size)
{
    if (!m_free_head)
    {
        return nullptr;
    }

    ++m_action_id;

    const uint32_t total_size =
        (uint32_t)math_utils::align_as_pow2(size, 16) + gen_allocator::s_block_size;

    auto start = m_free_head;

    do
    {
        if (m_free_head->size >= total_size)
        {
            if (m_free_head->size - total_size >= (gen_allocator::s_block_size + 8))
            {
                auto parent_block = *m_free_head;
                auto size_before_split = m_free_head->size;

                auto split_head = make_header(m_free_head, total_size);
                split_head->size = size_before_split - (uint32_t)total_size;
                split_head->block_id = m_block_id++;
                split_head->header = kHEADER;
                split_head->free = true;

                insert_freenode(m_free_head, m_free_head->free_next, split_head);
                insert_allnode(m_free_head, m_free_head->next, split_head);

                m_free_head->size = total_size;

                if (m_tracker)
                {
                    m_tracker->on_split(m_action_id, parent_block, *m_free_head, *split_head);
                }
            }

            auto to_delete = m_free_head;
            to_delete->free = false;

            m_free_head = m_free_head->free_next;

            delete_freenode(to_delete);

            if (m_tracker)
            {
                m_tracker->on_allocate(m_action_id, *to_delete, size);
            }

            return offset(to_delete, gen_allocator::s_block_size);
        }

        m_free_head = m_free_head->free_next;
    } while (m_free_head != start);

    return nullptr;
}

void
gen_allocator::release(void* ptr)
{
    if (!ptr)
    {
        return;
    }
    ++m_action_id;

    auto to_release = (memdescr*)offset(ptr, (int64_t)gen_allocator::s_block_size * (-1));

    if (to_release->header != kHEADER)
    {
        return;
    }

    to_release->free = true;

    if (m_tracker)
    {
        m_tracker->on_release(m_action_id, *m_all_head, ptr);
    }

    if (!m_free_head)
    {
        restore_freehead(to_release);
    }
    else
    {
        insert_freenode(m_free_head->free_prev, m_free_head->free_next, to_release);
    }

    if (to_release->prev->free && mergable(to_release->prev, to_release))
    {
        auto left = to_release->prev;
        auto right = to_release;

        auto left_desc = *left;
        auto right_desc = *right;

        left->size += to_release->size;
        delete_allnode(right);
        delete_freenode(right);

        if (m_tracker)
        {
            m_tracker->on_merge(m_action_id, left_desc, right_desc, *to_release->prev);
        }
    }

    if (to_release->next->free && mergable(to_release, to_release->next))
    {
        auto left = to_release;
        auto right = to_release->next;

        auto left_desc = *left;
        auto right_desc = *right;

        to_release->size += to_release->next->size;

        delete_allnode(right);
        delete_freenode(right);

        if (m_tracker)
        {
            m_tracker->on_merge(m_action_id, left_desc, right_desc, *to_release);
        }
    }
}

bool
gen_allocator::mergable(memdescr* left, memdescr* right)
{
    auto ptr = offset(left, left->size);

    return ptr == right;
}

void
gen_allocator::delete_allnode(gen_allocator::memdescr* n)
{
    auto left = n->prev;
    auto right = n->next;

    left->next = right;
    right->prev = left;

    if (n == m_all_head)
    {
        m_all_head = n->prev;
    }
}

void
gen_allocator::delete_freenode(gen_allocator::memdescr* n)
{
    auto left = n->free_prev;
    auto right = n->free_next;

    left->free_next = right;
    right->free_prev = left;

    n->free_prev = nullptr;
    n->free_next = nullptr;

    if (n == m_free_head)
    {
        m_free_head = left != m_free_head ? left : nullptr;
    }
}

void
gen_allocator::insert_allnode(gen_allocator::memdescr* l,
                              gen_allocator::memdescr* r,
                              gen_allocator::memdescr* n)
{
    l->next = n;
    n->prev = l;

    r->prev = n;
    n->next = r;
}

void
gen_allocator::insert_freenode(gen_allocator::memdescr* l,
                               gen_allocator::memdescr* r,
                               gen_allocator::memdescr* n)
{
    l->free_next = n;
    n->free_prev = l;

    r->free_prev = n;
    n->free_next = r;
}

void
gen_allocator::restore_freehead(gen_allocator::memdescr* n)
{
    m_free_head = n;
    m_free_head->free_next = m_free_head;
    m_free_head->free_prev = m_free_head;
}

void
gen_allocator::print_free()
{
    std::cout << "Free stats:" << std::endl;

    auto start = m_free_head;
    auto head = m_free_head;

    if (!start)
    {
        std::cout << "Empty" << std::endl;
        return;
    }

    do
    {
        std::cout << std::format("id: {}  size: {} free: {} id: {}",
                                 (uint64_t)head - (uint64_t)m_data, head->size, head->free,
                                 head->block_id)
                  << std::endl;

        head = head->free_next;

    } while (head != start);
}

void
gen_allocator::print_all()
{
    std::cout << "All stats:" << std::endl;

    auto start = m_all_head;
    auto head = m_all_head;

    if (!start)
    {
        std::cout << "Empty" << std::endl;
        return;
    }

    do
    {
        std::cout << std::format("id: {}  size: {} free: {} id: {}",
                                 (uint64_t)head - (uint64_t)m_data, head->size, head->free,
                                 head->block_id)
                  << std::endl;

        head = head->next;

    } while (head != start);
}

}  // namespace agea::utils
