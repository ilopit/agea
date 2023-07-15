#pragma once

#include <vector>

namespace agea
{
namespace utils
{
template <typename T>
class line_conteiner
{
    using itr = std::vector<T>::iterator;
    using citr = std::vector<T>::const_iterator;
    using item_type = T;

public:
    uint64_t
    get_size() const
    {
        return (uint64_t)m_items.size();
    }

    template <class... U>
    constexpr decltype(auto)
    emplace_back(U&&... _Val)
    {
        m_items.emplace_back(std::forward<U>(_Val)...);
    }

    void
    swap_and_remove(itr pos)
    {
        AGEA_check(!m_items.empty(), "Should not be empty!");
        AGEA_check(pos >= m_items.begin(), "Should not be empty!");

        std::swap(*m_items.rbegin(), *pos);
        m_items.pop_back();
    }

    _NODISCARD constexpr T&
    operator[](const uint64_t idx) noexcept
    {
        return m_items[idx];
    }

    _NODISCARD constexpr const T&
    operator[](const uint64_t idx) const noexcept
    {
        return m_items[idx];
    }

    itr
    find(const T& v)
    {
        return std::find(m_items.begin(), m_items.end(), v);
    }

    template <typename pred>
    itr
    find_if(pred p)
    {
        return std::find_if(m_items.begin(), m_items.end(), p);
    }

    _NODISCARD constexpr itr
    begin() noexcept
    {
        return m_items.begin();
    }

    _NODISCARD constexpr itr
    end() noexcept
    {
        return m_items.end();
    }

    _NODISCARD constexpr citr
    begin() const noexcept
    {
        return m_items.begin();
    }

    _NODISCARD constexpr citr
    end() const noexcept
    {
        return m_items.end();
    }

    _NODISCARD constexpr citr
    cbegin() const noexcept
    {
        return m_items.begin();
    }

    _NODISCARD constexpr citr
    cend() const noexcept
    {
        return m_items.end();
    }

    T&
    front()
    {
        return m_items.front();
    }

    void
    clear()
    {
        m_items.clear();
    }

    bool
    empty() const
    {
        return m_items.empty();
    }

protected:
    std::vector<T> m_items;
};
}  // namespace utils

}  // namespace agea
