#pragma once

#include <string>
#include <string_view>

namespace kryga
{
namespace vfs
{

// Resource identifier — a pre-parsed virtual path.
// Constructed via vfs::rid("mount://relative") or vfs::rid("mount", "relative").
class rid
{
public:
    constexpr rid() = default;

    // From full vpath: "data://configs/kryga.acfg"
    constexpr explicit rid(std::string_view vpath)
    {
        auto sep = vpath.find("://");
        if(sep != std::string_view::npos)
        {
            m_mount_point = vpath.substr(0, sep);
            m_relative = vpath.substr(sep + 3);
        }
        else
        {
            m_mount_point = vpath;
        }
    }

    // From components: ("data", "configs/kryga.acfg")
    constexpr rid(std::string_view mount_point, std::string_view relative)
        : m_mount_point(mount_point)
        , m_relative(relative)
    {
    }

    constexpr std::string_view
    mount_point() const
    {
        return m_mount_point;
    }

    constexpr std::string_view
    relative() const
    {
        return m_relative;
    }

    constexpr std::string
    str() const
    {
        std::string result;
        result.reserve(m_mount_point.size() + 3 + m_relative.size());
        result.append(m_mount_point);
        result.append("://");
        result.append(m_relative);
        return result;
    }

    constexpr bool
    empty() const
    {
        return m_mount_point.empty();
    }

    // Append a path segment: rid("data://pkg") / "sub" -> rid("data", "pkg/sub")
    constexpr rid
    operator/(std::string_view segment) const
    {
        rid copy = *this;
        copy /= segment;
        return copy;
    }

    constexpr rid&
    operator/=(std::string_view segment)
    {
        if(!m_relative.empty() && m_relative.back() != '/')
        {
            m_relative += '/';
        }
        m_relative.append(segment);
        return *this;
    }

    constexpr bool
    operator==(const rid& other) const
    {
        return m_mount_point == other.m_mount_point && m_relative == other.m_relative;
    }

    constexpr bool
    operator!=(const rid& other) const
    {
        return !(*this == other);
    }

private:
    std::string m_mount_point;
    std::string m_relative;
};

}  // namespace vfs
}  // namespace kryga

namespace std
{
template <>
struct hash<::kryga::vfs::rid>
{
    size_t
    operator()(const ::kryga::vfs::rid& r) const
    {
        auto h1 = std::hash<std::string_view>{}(r.mount_point());
        auto h2 = std::hash<std::string_view>{}(r.relative());
        return h1 ^ (h2 << 1);
    }
};
}  // namespace std
