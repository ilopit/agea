#include "vfs/file_index.h"

#include "vfs/vfs.h"
#include "vfs/rid.h"

#include <utils/kryga_log.h>

namespace kryga
{
namespace vfs
{

bool
file_index::build(const std::filesystem::path& root, std::string_view ext_filter)
{
    std::error_code ec;
    if (!std::filesystem::exists(root, ec))
    {
        ALOG_ERROR("file_index: directory does not exist: {}", root.generic_string());
        return false;
    }

    auto it = std::filesystem::recursive_directory_iterator(root, ec);
    if (ec)
    {
        ALOG_ERROR("file_index: failed to iterate: {}", root.generic_string());
        return false;
    }

    for (auto& entry : it)
    {
        if (entry.is_directory())
        {
            continue;
        }

        if (!ext_filter.empty())
        {
            auto ext = entry.path().extension().generic_string();
            if (ext != ext_filter)
            {
                continue;
            }
        }

        auto rel = std::filesystem::relative(entry.path(), root, ec).generic_string();
        if (ec)
        {
            continue;
        }

        auto stem = entry.path().stem().generic_string();

        auto [_, inserted] = m_entries.try_emplace(stem, rel);
        if (!inserted)
        {
            ALOG_ERROR(
                "file_index: duplicate stem '{}' — '{}' vs '{}'", stem, m_entries[stem], rel);
            return false;
        }
    }

    rebuild_ordered();
    return true;
}

bool
file_index::build(const virtual_file_system& vfs, const rid& root, std::string_view ext_filter)
{
    bool duplicate = false;

    bool ok = vfs.enumerate(
        root,
        [&](std::string_view path, bool is_dir) -> bool
        {
            if (is_dir)
            {
                return true;
            }

            // Extract stem from path
            auto slash = path.rfind('/');
            auto filename = (slash != std::string_view::npos) ? path.substr(slash + 1) : path;
            auto dot = filename.rfind('.');
            if (dot == std::string_view::npos)
            {
                return true;
            }

            auto stem = std::string(filename.substr(0, dot));
            auto rel = std::string(path);

            auto [_, inserted] = m_entries.try_emplace(stem, rel);
            if (!inserted)
            {
                ALOG_ERROR(
                    "file_index: duplicate stem '{}' — '{}' vs '{}'", stem, m_entries[stem], rel);
                duplicate = true;
                return false;
            }

            return true;
        },
        true,
        ext_filter);

    if (ok && !duplicate)
    {
        rebuild_ordered();
    }
    return ok && !duplicate;
}

bool
file_index::add(std::string_view name, std::string_view relative_path)
{
    auto [_, inserted] = m_entries.try_emplace(std::string(name), std::string(relative_path));
    if (!inserted)
    {
        ALOG_ERROR("file_index: duplicate entry '{}'", name);
        return false;
    }
    m_ordered.emplace_back(std::string(name), std::string(relative_path));
    return true;
}

bool
file_index::resolve(std::string_view name, std::string& out_relative) const
{
    auto it = m_entries.find(std::string(name));
    if (it == m_entries.end())
    {
        return false;
    }
    out_relative = it->second;
    return true;
}

bool
file_index::contains(std::string_view name) const
{
    return m_entries.contains(std::string(name));
}

void
file_index::set_load_order(const std::vector<std::string>& prefixes)
{
    m_load_order = prefixes;
    rebuild_ordered();
}

void
file_index::clear()
{
    m_entries.clear();
    m_ordered.clear();
}

void
file_index::rebuild_ordered()
{
    m_ordered.clear();
    m_ordered.reserve(m_entries.size());
    for (auto& [name, path] : m_entries)
    {
        m_ordered.emplace_back(name, path);
    }

    if (m_load_order.empty())
    {
        return;
    }

    std::stable_sort(m_ordered.begin(),
                     m_ordered.end(),
                     [&](const auto& a, const auto& b)
                     {
                         auto rank = [&](const std::string& path) -> int
                         {
                             for (int i = 0; i < static_cast<int>(m_load_order.size()); ++i)
                             {
                                 if (path.starts_with(m_load_order[i]))
                                 {
                                     return i;
                                 }
                             }
                             return static_cast<int>(m_load_order.size());
                         };
                         return rank(a.second) < rank(b.second);
                     });
}

}  // namespace vfs
}  // namespace kryga
