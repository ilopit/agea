#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <vector>

namespace kryga
{
namespace utils
{

// Lock-free single-producer single-consumer ring buffer.
// Producer and consumer must be on separate threads (or the same thread for testing).
// T must be moveable.
template <typename T>
class spsc_queue
{
    static constexpr size_t k_cache_line = 64;

public:
    explicit spsc_queue(size_t capacity)
        : m_capacity(capacity + 1)  // One slot wasted to distinguish full from empty
        , m_buffer(m_capacity)
    {
    }

    spsc_queue(const spsc_queue&) = delete;
    spsc_queue&
    operator=(const spsc_queue&) = delete;

    // Producer: push an item. Returns false if full.
    bool
    try_push(T&& item)
    {
        const auto head = m_head.load(std::memory_order_relaxed);
        const auto next = advance(head);

        if (next == m_tail.load(std::memory_order_acquire))
        {
            return false;  // Full
        }

        m_buffer[head] = std::move(item);
        m_head.store(next, std::memory_order_release);
        return true;
    }

    // Producer: push an item, block-spins if full.
    void
    push(T&& item)
    {
        while (!try_push(std::move(item)))
        {
            // Spin — backpressure
        }
    }

    // Consumer: pop an item. Returns nullopt if empty.
    std::optional<T>
    try_pop()
    {
        const auto tail = m_tail.load(std::memory_order_relaxed);

        if (tail == m_head.load(std::memory_order_acquire))
        {
            return std::nullopt;  // Empty
        }

        T item = std::move(m_buffer[tail]);
        m_tail.store(advance(tail), std::memory_order_release);
        return item;
    }

    // Consumer: drain all available items into a callback.
    // Returns number of items processed.
    template <typename Fn>
    size_t
    drain(Fn&& fn)
    {
        size_t count = 0;
        while (auto item = try_pop())
        {
            fn(std::move(*item));
            ++count;
        }
        return count;
    }

    bool
    empty() const
    {
        return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
    }

    size_t
    capacity() const
    {
        return m_capacity - 1;
    }

private:
    size_t
    advance(size_t idx) const
    {
        return (idx + 1) % m_capacity;
    }

    const size_t m_capacity;

    // Pad to avoid false sharing between producer (head) and consumer (tail)
    alignas(k_cache_line) std::atomic<size_t> m_head{0};
    alignas(k_cache_line) std::atomic<size_t> m_tail{0};

    std::vector<T> m_buffer;
};

}  // namespace utils
}  // namespace kryga
