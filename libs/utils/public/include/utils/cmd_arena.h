#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

namespace kryga
{
namespace utils
{

// Linear arena for per-frame command allocation.
// Allocate via alloc<T>(), bulk reclaim via reset().
class cmd_arena
{
    static constexpr size_t k_default_capacity = 4 * 1024 * 1024;  // 4 MB

public:
    explicit cmd_arena(size_t capacity = k_default_capacity)
        : m_buf(capacity)
    {
    }

    void*
    alloc_raw(size_t size, size_t align)
    {
        size_t aligned = (m_offset + align - 1) & ~(align - 1);
        assert(aligned + size <= m_buf.size());

        void* ptr = m_buf.data() + aligned;
        m_offset = aligned + size;
        return ptr;
    }

    template <typename T, typename... Args>
    T*
    alloc(Args&&... args)
    {
        void* ptr = alloc_raw(sizeof(T), alignof(T));
        return new (ptr) T(std::forward<Args>(args)...);
    }

    void
    reset()
    {
        m_offset = 0;
    }

    size_t
    used() const
    {
        return m_offset;
    }

    size_t
    capacity() const
    {
        return m_buf.size();
    }

private:
    std::vector<std::byte> m_buf;
    size_t m_offset = 0;
};

}  // namespace utils
}  // namespace kryga
