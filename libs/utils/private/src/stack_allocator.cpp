#include "utils/stack_allocator.h"

#include <stdlib.h>
#include <cstring>

namespace agea::utils
{
stack_allocator::stack_allocator(uint64_t size)
    : m_sysmem((uint8_t*)malloc(size))
    , m_sysmem_end(m_sysmem + size)
    , m_last((memdescr*)m_sysmem)
{
    memset(m_sysmem, 0, 16);
}

void
stack_allocator::dealloc(void* void_ptr)
{
    if (!void_ptr)
    {
        return;
    }

    auto ptr = (uint8_t*)void_ptr;

    memdescr* dd = desc(ptr);
    dd->size = 0;
    dd->items_count = 0;
    dd->flags = 0;

    while (((uint8_t*)m_last != m_sysmem) && m_last->size == 0)
    {
        m_last = m_last->prev;
    }
}

agea::utils::stack_allocator::memdescr*
stack_allocator::next_desc(uint32_t size)
{
    auto new_pos = m_last->size + (uint8_t*)m_last;
    auto left = m_sysmem_end - new_pos;

    return (left > size) ? (memdescr*)new_pos : nullptr;
}

agea::utils::stack_allocator::memdescr*
stack_allocator::desc(uint32_t offst) const
{
    return (memdescr*)(m_sysmem + offst);
}

agea::utils::stack_allocator::memdescr*
stack_allocator::desc(uint8_t* ptr)
{
    return (memdescr*)(ptr - sizeof(memdescr));
}

void*
stack_allocator::alloc_array(uint32_t item_size, uint32_t count)
{
    const uint32_t al_size =
        (uint32_t)math_utils::align_as_pow2(item_size * count, sizeof(memdescr));

    auto md = next_desc(al_size);
    if (!md)
    {
        return nullptr;
    }

    md->size = al_size + sizeof(memdescr);
    md->prev = m_last;
    md->flags = sa_flags::f_array;
    md->items_count = count;

    return data_ptr();
}

void*
stack_allocator::alloc_item(uint32_t size)
{
    const uint32_t al_size = (uint32_t)math_utils::align_as_pow2(size, sizeof(memdescr));

    auto md = next_desc(al_size);
    if (!md)
    {
        return nullptr;
    }

    md->size = al_size + sizeof(memdescr);
    md->prev = m_last;
    md->flags = sa_flags::f_item;
    md->items_count = 0;

    m_last = md;

    return data_ptr();
}

void*
stack_allocator::data_ptr()
{
    return ((uint8_t*)m_last + sizeof(memdescr));
}

uint8_t*
stack_allocator::sysmem()
{
    return m_sysmem;
}

uint32_t
stack_allocator::used() const
{
    return ((uint8_t*)m_last - m_sysmem) + m_last->size;
}

bool
stack_allocator::owner(uint8_t* ptr) const
{
    return m_sysmem >= ptr && ptr < m_sysmem_end;
}

}  // namespace agea::utils