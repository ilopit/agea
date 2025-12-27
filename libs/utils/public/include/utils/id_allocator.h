#pragma once

#include <cstdint>
#include <vector>

namespace agea
{
namespace utils
{
class id_allocator
{
public:
    uint64_t
    alloc_id()
    {
        if (!m_free_ids.empty())
        {
            auto pos = m_free_ids.back();
            m_free_ids.pop_back();

            return pos;
        }

        return m_ids_in_fly++;
    }

    uint64_t
    get_ids_in_fly() const
    {
        return m_ids_in_fly;
    }

    void
    release_id(uint64_t id)
    {
        m_free_ids.push_back(id);
    }

private:
    uint64_t m_ids_in_fly = 0;
    std::vector<uint64_t> m_free_ids;
};
}  // namespace utils
}  // namespace agea