#pragma once

#include <cstdint>

#include <utils/math_utils.h>

namespace agea::utils
{

struct allocation_tracker
{
    virtual void
    on_alloc_item(uint32_t size)
    {
    }

    virtual void
    on_alloc_array(uint32_t item_size, uint32_t count)
    {
    }
};

class stack_allocator
{
    enum sa_flags : uint32_t
    {
        f_nan = 0,
        f_item = 1,
        f_array = 2
    };

    struct memdescr
    {
        memdescr* prev = nullptr;
        uint32_t flags : 2 = sa_flags::f_nan;
        uint32_t items_count : 30 = 0;
        uint32_t size = 0;
    };

public:
    stack_allocator(uint64_t size);

    void*
    alloc_item(uint32_t size);

    void*
    alloc_array(uint32_t item_size, uint32_t count);

    void*
    data_ptr();

    memdescr*
    desc(uint8_t* ptr);

    memdescr*
    desc(uint32_t offset) const;

    memdescr*
    next_desc(uint32_t size);

    void
    dealloc(void* void_ptr);

    uint8_t*
    sysmem();

    uint32_t
    used() const;

    bool
    owner(uint8_t* ptr) const;

private:
    uint8_t* m_sysmem = nullptr;
    uint8_t* m_sysmem_end = nullptr;
    memdescr* m_last = nullptr;
};

}  // namespace agea::utils
