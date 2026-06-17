#pragma once

#include <cstdint>
#include <vector>

namespace kryga
{
namespace utils
{

// Slot + generation handle allocator. Hands back opaque uint64 handles encoded as
// (index | generation << 32); resolve() rejects a handle whose generation no longer
// matches its slot, so a stale handle to a freed-then-reused slot reads as invalid
// instead of silently aliasing the new occupant.
//
// Generations start at 1 and never reach 0 again, so a live handle's value is always
// non-zero — value == 0 is reserved as "invalid" (matches physics::*_handle).
template <typename T>
class handle_pool
{
public:
    // Reserve a slot and return its handle. The payload is default-constructed;
    // fill it later with bind(). Reuses a freed slot when available.
    uint64_t
    alloc()
    {
        uint32_t index;
        if (!m_free.empty())
        {
            index = m_free.back();
            m_free.pop_back();
            m_slots[index].alive = true;
            m_slots[index].payload = T{};
        }
        else
        {
            index = static_cast<uint32_t>(m_slots.size());
            m_slots.push_back(slot{T{}, 1u, true});
        }
        return encode(index, m_slots[index].generation);
    }

    // Resolve a handle to its payload, or nullptr if the handle is invalid / stale.
    T*
    resolve(uint64_t handle)
    {
        slot* s = slot_of(handle);
        return s ? &s->payload : nullptr;
    }

    // Store the payload for a previously allocated handle. No-op on a stale handle.
    void
    bind(uint64_t handle, T value)
    {
        if (slot* s = slot_of(handle))
        {
            s->payload = std::move(value);
        }
    }

    // Visit the payload of every live slot. Order is unspecified. Useful for bulk
    // teardown (e.g. destroying all bodies on shutdown).
    template <typename Fn>
    void
    for_each_alive(Fn&& fn)
    {
        for (auto& s : m_slots)
        {
            if (s.alive)
            {
                fn(s.payload);
            }
        }
    }

    // Drop all slots and free lists. Does NOT visit payloads — call for_each_alive
    // first if they own resources.
    void
    clear()
    {
        m_slots.clear();
        m_free.clear();
    }

    // Release a handle's slot and bump its generation so the old handle goes stale.
    // No-op on an invalid / already-freed handle.
    void
    free(uint64_t handle)
    {
        slot* s = slot_of(handle);
        if (!s)
        {
            return;
        }
        s->alive = false;
        // Skip 0 on wraparound so freed generations never produce a zero handle.
        s->generation = s->generation + 1u == 0u ? 1u : s->generation + 1u;
        m_free.push_back(index_of(handle));
    }

private:
    struct slot
    {
        T payload;
        uint32_t generation = 1u;
        bool alive = false;
    };

    static uint64_t
    encode(uint32_t index, uint32_t generation)
    {
        return static_cast<uint64_t>(index) | (static_cast<uint64_t>(generation) << 32);
    }

    static uint32_t
    index_of(uint64_t handle)
    {
        return static_cast<uint32_t>(handle & 0xFFFFFFFFu);
    }

    static uint32_t
    generation_of(uint64_t handle)
    {
        return static_cast<uint32_t>(handle >> 32);
    }

    slot*
    slot_of(uint64_t handle)
    {
        if (handle == 0)
        {
            return nullptr;
        }
        uint32_t index = index_of(handle);
        if (index >= m_slots.size())
        {
            return nullptr;
        }
        slot& s = m_slots[index];
        if (!s.alive || s.generation != generation_of(handle))
        {
            return nullptr;
        }
        return &s;
    }

    std::vector<slot> m_slots;
    std::vector<uint32_t> m_free;
};

}  // namespace utils
}  // namespace kryga
