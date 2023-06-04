#pragma once

#include "utils/path.h"
#include "utils/file_utils.h"

#include <vector>
#include <string>

namespace agea
{
namespace utils
{
struct buffer;

template <typename T>
struct buffer_view
{
    explicit buffer_view(buffer& b)
        : m_ref(&b)
    {
    }

    T*
    as()
    {
        return (T*)m_ref->data();
    }

    T&
    at(uint32_t idx)
    {
        return *(as() + idx);
    }

    uint64_t
    size() const
    {
        return uint64_t(m_ref->size() / sizeof(T));
    }

    uint64_t
    size_bytes() const
    {
        return m_ref->size();
    }

    void
    resize(uint64_t new_size)
    {
        m_ref->resize(new_size * sizeof(T));
    }

    uint8_t*
    data()
    {
        return m_ref->data();
    }

private:
    buffer* m_ref = nullptr;
};

struct buffer
{
    static bool
    load(const utils::path& p, buffer& b);

    template <typename T>
    buffer_view<T>
    make_view()
    {
        return buffer_view<T>(*this);
    }

    buffer() = default;

    buffer(size_t s);

    buffer(size_t s, uint8_t value);

    buffer(const buffer& other) = default;

    buffer&
    operator=(const buffer& other) = default;

    buffer(buffer&& other) noexcept
        : m_file(std::move(other.m_file))
        , m_data(std::move(other.m_data))
        , m_last_write_time(other.m_last_write_time)
    {
        other.m_last_write_time = std::filesystem::file_time_type{};
    }

    buffer&
    operator=(buffer&& other) noexcept
    {
        if (this != &other)
        {
            m_file = std::move(other.m_file);
            m_data = std::move(other.m_data);
            m_last_write_time = other.m_last_write_time;

            other.m_last_write_time = std::filesystem::file_time_type{};
        }

        return *this;
    }

    static bool
    save(buffer& b)
    {
        return file_utils::save_file(b.m_file, b.m_data);
    }

    void
    set_file(const utils::path& p)
    {
        m_file = p;
    }

    const utils::path&
    get_file() const
    {
        return m_file;
    }

    uint8_t*
    data()
    {
        return m_data.data();
    }

    const uint8_t*
    data() const
    {
        return m_data.data();
    }

    void
    write(const uint8_t* data, uint64_t size)
    {
        auto old_size = m_data.size();

        m_data.resize(m_data.size() + size);
        memcpy(m_data.data() + old_size, data, size);
    }

    std::vector<uint8_t>&
    full_data()
    {
        return m_data;
    }

    uint64_t
    size() const
    {
        return m_data.size();
    }

    void
    resize(uint64_t new_size)
    {
        m_data.resize(new_size);
    }

    bool
    has_file_updated() const;

    bool
    consume_file_updated();

private:
    utils::path m_file;
    std::vector<uint8_t> m_data;
    std::filesystem::file_time_type m_last_write_time;
};

}  // namespace utils
}  // namespace agea