#pragma once

#include <cstdint>

#include <utils/math_utils.h>

namespace agea::utils
{

class gen_allocator
{
public:
    struct memdescr
    {
        uint16_t header = 0;
        uint8_t block_id = 0;
        bool free = 0;
        uint32_t size = 0;

        memdescr* prev = nullptr;
        memdescr* next = nullptr;

        memdescr* free_prev = nullptr;
        memdescr* free_next = nullptr;
    };

    static constexpr uint32_t s_block_size = sizeof(memdescr);

    struct tracker
    {
        virtual void
        on_allocate(uint32_t action_id, const memdescr& block, uint32_t requested) = 0;

        virtual void
        on_release(uint32_t action_id, const memdescr& block, void* requested) = 0;

        virtual void
        on_split(uint32_t action_id,
                 const memdescr& parent,
                 const memdescr& left,
                 const memdescr& right) = 0;

        virtual void
        on_merge(uint32_t action_id,
                 const memdescr& left,
                 const memdescr& right,
                 const memdescr& result) = 0;
    };

    gen_allocator(uint64_t size, tracker* t = nullptr);

    void*
    alloc(uint32_t size);

    void
    release(void* ptr);

    bool
    mergable(memdescr* left, memdescr* right);

    void
    insert_freenode(gen_allocator::memdescr* l,
                    gen_allocator::memdescr* r,
                    gen_allocator::memdescr* n);

    void
    insert_allnode(gen_allocator::memdescr* l,
                   gen_allocator::memdescr* r,
                   gen_allocator::memdescr* n);

    void
    delete_freenode(gen_allocator::memdescr* n);

    void
    delete_allnode(gen_allocator::memdescr* n);

    void
    restore_freehead(gen_allocator::memdescr* n);

    const uint64_t m_alloc_size;
    uint8_t* m_data = nullptr;
    memdescr* m_all_head = nullptr;
    memdescr* m_free_head = nullptr;

    tracker* m_tracker = nullptr;
    uint8_t m_block_id = 0;
    uint32_t m_action_id = 0;

    void
    print_all();

    void
    print_free();
};

}  // namespace agea::utils
