#pragma once

#include <deque>
#include <unordered_map>

#include <utils/id.h>
#include <utils/agea_log.h>

namespace agea
{
namespace utils
{

template <typename T>
class combined_pool
{
public:
    T*
    alloc(const utils::id& id)
    {
        auto result = m_mapping.insert(std::pair<utils::id, T*>{id, nullptr});

        if (!result.second)
        {
            ALOG_ERROR("[{0}] already allocated", id.cstr());
            return nullptr;
        }

        T* obj = result.first->second;

        if (!m_free_slots.empty())
        {
            auto slot = m_free_slots.back();
            m_free_slots.pop_back();

            obj = &m_items.at(slot);

            obj = new (obj) T(id, slot);
        }
        else
        {
            auto idx = (uint32_t)m_items.size();
            obj = &m_items.emplace_back(id, idx);
        }

        return obj;
    }

    uint64_t
    get_size() const
    {
        return m_items.size();
    }

    T*
    at(uint32_t slot)
    {
        return &m_items.at((size_t)slot);
    }

    void
    release(T* obj)
    {
        auto itr = m_mapping.find(obj->id());

        if (itr == m_mapping.end())
        {
            ALOG_ERROR("[{0}] already allocated", obj->id().cstr());
            return;
        }

        m_mapping.erase(itr);

        auto slot_id = obj->slot();

        obj->~T();

        m_free_slots.push_back(slot_id);
    }

private:
    std::deque<T> m_items;

    std::deque<uint32_t> m_free_slots;
    std::unordered_map<utils::id, T*> m_mapping;
};
}  // namespace utils
}  // namespace agea