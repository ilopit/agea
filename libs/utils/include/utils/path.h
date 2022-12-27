#pragma once

#include <filesystem>

#include "utils/check.h"

namespace agea
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
        AGEA_check(c, "Should not be NULL");
        normalize();
    }

    explicit path(const std::filesystem::path& p)
        : m_value(p)
    {
        normalize();
    }

    path() = default;

    path(const path&) = default;

    path(path&&) = default;

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
        AGEA_check(p, "Should not be NULL");

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
        AGEA_check(p, "Should not be NULL");
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
    has_extention(const char* v) const
    {
        auto e = m_value.extension().generic_string();
        return e.compare(v) == 0;
    }

    bool
    operator<(const agea::utils::path& l) const
    {
        return fs() < l.fs();
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

}  // namespace agea

inline agea::utils::path
operator/(const agea::utils::path& l, const agea::utils::path& r)
{  // append a pair of paths together
    agea::utils::path tmp = l;
    return tmp.append(r);
}

inline agea::utils::path
operator/(const agea::utils::path& l, const char* r)
{  // append a pair of paths together
    agea::utils::path tmp = l;
    return tmp.append(r);
}

inline agea::utils::path
operator/(const agea::utils::path& l, const std::string& r)
{  // append a pair of paths together
    agea::utils::path tmp = l;
    return tmp.append(r);
}

inline bool
operator<(const ::agea::utils::path& l, const ::agea::utils::path& r)
{
    return l.fs() < r.fs();
}

#define APATH(value) ::agea::utils::path(value)