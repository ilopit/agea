#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace kryga
{

struct fnv_hasher
{
    size_t h = 14695981039346656037ull;

    void
    feed(const void* data, size_t len)
    {
        auto* p = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < len; ++i)
        {
            h ^= size_t(p[i]);
            h *= 1099511628211ull;
        }
    }

    void
    feed_str(std::string_view s)
    {
        feed(s.data(), s.size());
    }

    size_t
    value() const
    {
        return h;
    }

    std::string
    hex() const
    {
        char buf[17];
        std::snprintf(buf, sizeof(buf), "%016zx", h);
        return std::string(buf);
    }
};

inline size_t
fnv_hash(const void* data, size_t len)
{
    fnv_hasher fh;
    fh.feed(data, len);
    return fh.value();
}

inline size_t
fnv_hash(std::string_view s)
{
    return fnv_hash(s.data(), s.size());
}

}  // namespace kryga
