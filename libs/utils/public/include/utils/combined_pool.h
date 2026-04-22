#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

#include <utils/id.h>
#include <utils/kryga_log.h>
#include <utils/slot_handle.h>

namespace kryga
{
namespace utils
{

template <typename T>
class combined_pool
{
public:
    using handle = slot_handle<T>;

    combined_pool() = default;

    combined_pool(bool offset)
    {
        (void)offset;
        m_items.emplace_back(AID("INVALID"), 0);
        m_generations.emplace_back(0);
        m_alive.emplace_back(false);
    }

    // ---------- Legacy id-keyed API (transitional) ----------

    template <typename... Args>
    T*
    alloc(const utils::id& id, Args&&... args)
    {
        auto result = m_mapping.insert(std::pair<utils::id, T*>{id, nullptr});

        if (!result.second)
        {
            ALOG_ERROR("[{0}] already allocated", id.cstr());
            return nullptr;
        }

        T* obj = nullptr;
        ++m_active_size;
        if (!m_free_slots.empty())
        {
            auto slot = m_free_slots.back();
            m_free_slots.pop_back();

            obj = &m_items.at(slot);
            obj = new (obj) T(id, slot, std::forward<Args>(args)...);
            m_alive[slot] = true;
        }
        else
        {
            auto idx = (uint32_t)m_items.size();
            obj = &m_items.emplace_back(id, idx, std::forward<Args>(args)...);
            m_generations.emplace_back(0);
            m_alive.emplace_back(true);
        }

        result.first->second = obj;
        return obj;
    }

    T*
    find_by_id(const utils::id& id)
    {
        auto itr = m_mapping.find(id);
        return itr != m_mapping.end() ? itr->second : nullptr;
    }

    void
    release(T* obj)
    {
        auto itr = m_mapping.find(obj->id());

        if (itr == m_mapping.end())
        {
            ALOG_ERROR("[{0}] not found in pool", obj->id().cstr());
            return;
        }
        --m_active_size;
        m_mapping.erase(itr);

        auto slot_id = obj->slot();

        obj->~T();

        m_alive[slot_id] = false;
        ++m_generations[slot_id];
        m_free_slots.push_back(slot_id);
    }

    // ---------- Slot-handle API (new) ----------

    // Model-thread: reserve a slot without constructing T. Pops from the
    // pre-handed batch or atomically bumps the overflow counter.
    handle
    alloc_handle()
    {
        if (!m_model_batch.empty())
        {
            auto h = m_model_batch.back();
            m_model_batch.pop_back();
            return h;
        }
        // Overflow path: reserve a fresh slot ahead of storage.
        uint32_t slot = m_high_water.fetch_add(1, std::memory_order_relaxed);
        return handle{slot, 0};
    }

    // Render-thread: construct T in the reserved slot. Grows storage if
    // the handle refers to a slot beyond current capacity.
    template <typename... Args>
    T*
    materialize(handle h, const utils::id& id, Args&&... args)
    {
        const uint32_t slot = h.slot();
        ensure_slot(slot);

        // Reserving the handle guarantees the slot is not concurrently live.
        // If caller misuses the API (double-materialize), alive flag catches it.
        if (m_alive[slot])
        {
            ALOG_ERROR("[{0}] slot {1} already materialized", id.cstr(), slot);
            return &m_items[slot];
        }

        T* obj = &m_items[slot];
        obj = new (obj) T(id, slot, std::forward<Args>(args)...);
        m_alive[slot] = true;
        m_generations[slot] = h.gen();
        ++m_active_size;
        m_mapping[id] = obj;  // transitional — allows find_by_id during migration
        return obj;
    }

    // Render-thread: validated dereference.
    T*
    get(handle h)
    {
        const uint32_t slot = h.slot();
        if (slot >= m_items.size() || !m_alive[slot])
        {
            return nullptr;
        }
        if (m_generations[slot] != h.gen())
        {
            return nullptr;
        }
        return &m_items[slot];
    }

    // Render-thread: release a slot back to the free list, bump generation.
    void
    release_handle(handle h)
    {
        const uint32_t slot = h.slot();
        if (slot >= m_items.size() || !m_alive[slot])
        {
            return;
        }
        if (m_generations[slot] != h.gen())
        {
            return;
        }
        T* obj = &m_items[slot];
        auto old_id = obj->id();
        obj->~T();
        m_alive[slot] = false;
        ++m_generations[slot];
        m_free_slots.push_back(slot);
        --m_active_size;

        auto itr = m_mapping.find(old_id);
        if (itr != m_mapping.end())
        {
            m_mapping.erase(itr);
        }
    }

    // Render-thread: refill the model's batch with up to `target` handles.
    // Pops the free list first, then bumps the high-water mark.
    void
    refill_batch(size_t target)
    {
        while (m_model_batch.size() < target)
        {
            uint32_t slot;
            uint16_t gen;
            if (!m_free_slots.empty())
            {
                slot = m_free_slots.back();
                m_free_slots.pop_back();
                gen = m_generations[slot];
            }
            else
            {
                slot = m_high_water.fetch_add(1, std::memory_order_relaxed);
                gen = 0;
            }
            m_model_batch.emplace_back(slot, gen);
        }
    }

    // Render-thread: reclaim leftover handles not consumed by the model
    // (e.g., at shutdown or when the batch shrinks).
    void
    reclaim_batch()
    {
        for (auto h : m_model_batch)
        {
            m_free_slots.push_back(h.slot());
        }
        m_model_batch.clear();
    }

    // ---------- Iteration / size (unchanged) ----------

    uint64_t
    get_size() const
    {
        return m_items.size();
    }

    uint64_t
    get_actual_size() const
    {
        return m_active_size;
    }

    T*
    at(uint32_t slot)
    {
        return &m_items.at((size_t)slot);
    }

    void
    clear()
    {
        m_items.clear();
        m_free_slots.clear();
        m_mapping.clear();
        m_generations.clear();
        m_alive.clear();
        m_model_batch.clear();
        m_high_water.store(0, std::memory_order_relaxed);
        m_active_size = 0;
    }

private:
    void
    ensure_slot(uint32_t slot)
    {
        while (m_items.size() <= slot)
        {
            auto idx = static_cast<uint32_t>(m_items.size());
            // Grow with sentinel T (needed — T is not default-constructible),
            // then immediately destroy it so the slot is "dead memory", matching
            // the state after release_handle / legacy release.
            T& sentinel = m_items.emplace_back(AID("INVALID"), idx);
            sentinel.~T();
            m_generations.emplace_back(uint16_t{0});
            m_alive.emplace_back(uint8_t{0});
        }
    }

    std::deque<T> m_items;
    std::deque<uint16_t> m_generations;
    std::deque<uint8_t> m_alive;  // not bool — deque<bool> would be a packed bitset

    std::deque<uint32_t> m_free_slots;
    std::unordered_map<utils::id, T*> m_mapping;
    uint64_t m_active_size = 0;

    // Slot-handle machinery
    std::vector<handle> m_model_batch;
    std::atomic<uint32_t> m_high_water{0};
};
}  // namespace utils
}  // namespace kryga
