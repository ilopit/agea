#pragma once

#include "core/agea_minimal.h"

#include <filesystem>

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