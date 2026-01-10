#pragma once

#include "utils/check.h"

#include <filesystem>

namespace kryga
{
namespace utils
{
class path
{
public:
    explicit path(const std::string& s)
        : m_value(s)
    {
        normalize();
    }

    explicit path(const char* c)
        : m_value(c)
    {
        KRG_check(c, "Should not be NULL");
        normalize();
    }

    explicit path(const std::filesystem::path& p)
        : m_value(p)
    {
        normalize();
    }

    path() = default;

    path(const path&) = default;

    path(path&&) noexcept = default;

    path&
    operator=(const path&) = default;

    path&
    operator=(path&&) noexcept = default;

    path&
    operator/=(const path& other)
    {
        return append(other);
    }

    path&
    operator/=(const char* other)
    {
        return append(other);
    }

    path&
    operator/=(const std::string& other)
    {
        return append(other);
    }

    path&
    append(const path& p)
    {
        m_value /= p.m_value;
        normalize();
        return *this;
    }

    path&
    append(const char* p)
    {
        KRG_check(p, "Should not be NULL");

        m_value /= p;
        normalize();
        return *this;
    }

    path&
    append(const std::string& p)
    {
        m_value /= p;
        normalize();
        return *this;
    }

    path&
    add(const char* p)
    {
        KRG_check(p, "Should not be NULL");
        m_value += p;
        normalize();

        return *this;
    }

    path&
    add(const std::string& p)
    {
        m_value += p;
        normalize();

        return *this;
    }

    path
    relative(const path& p) const;

    path
    parent() const
    {
        return path(m_value.parent_path());
    }

    std::string
    str() const
    {
        return m_value.generic_string();
    }

    const std::filesystem::path&
    fs() const
    {
        return m_value;
    }

    bool
    exists() const;

    bool
    empty() const;

    std::string
    file_name() const
    {
        return m_value.filename().generic_string();
    }

    void
    parse_file_name_and_ext(std::string& file_name, std::string& extension) const
    {
        file_name = m_value.stem().generic_string();
        extension = m_value.extension().generic_string();
        if (!extension.empty() && extension.front() == '.')
        {
            extension = extension.substr(1);
        }
        else
        {
            extension.clear();
        }
    }

    bool
    has_extension(const char* v) const
    {
        auto e = m_value.extension().generic_string();
        return e.compare(v) == 0;
    }

    bool
    operator<(const kryga::utils::path& l) const
    {
        return fs() < l.fs();
    }

    bool
    operator==(const kryga::utils::path& l) const
    {
        return fs() == l.fs();
    }

private:
    void
    normalize()
    {
        m_value = m_value.lexically_normal();
    }

    std::filesystem::path m_value;
};
}  // namespace utils

}  // namespace kryga

inline kryga::utils::path
operator/(const kryga::utils::path& l, const kryga::utils::path& r)
{  // append a pair of paths together
    kryga::utils::path tmp = l;
    return tmp.append(r);
}

inline kryga::utils::path
operator/(const kryga::utils::path& l, const char* r)
{  // append a pair of paths together
    kryga::utils::path tmp = l;
    return tmp.append(r);
}

inline kryga::utils::path
operator/(const kryga::utils::path& l, const std::string& r)
{  // append a pair of paths together
    kryga::utils::path tmp = l;
    return tmp.append(r);
}

inline bool
operator<(const ::kryga::utils::path& l, const ::kryga::utils::path& r)
{
    return l.fs() < r.fs();
}

namespace std
{

template <>
struct hash<::kryga::utils::path>
{
    size_t
    operator()(const ::kryga::utils::path& k) const
    {
        return std::hash<std::string>{}(k.str());
    }
};

}  // namespace std

#define APATH(value) ::kryga::utils::path(value)