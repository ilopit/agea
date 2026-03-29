#include "vfs/memory_backend.h"

#include <algorithm>

namespace kryga
{
namespace vfs
{

std::string_view
memory_backend::name() const
{
    return "memory";
}

file_info
memory_backend::stat(std::string_view relative_path) const
{
    std::string key(relative_path);

    if (m_directories.contains(key))
    {
        file_info info;
        info.exists = true;
        info.is_directory = true;
        return info;
    }

    auto it = m_files.find(key);
    if (it == m_files.end())
        return {};

    file_info info;
    info.exists = true;
    info.is_directory = false;
    info.size = it->second.data.size();
    info.last_modified = it->second.last_modified;
    return info;
}

bool
memory_backend::read_all(std::string_view relative_path, std::vector<uint8_t>& out) const
{
    auto it = m_files.find(std::string(relative_path));
    if (it == m_files.end())
        return false;

    out = it->second.data;
    return true;
}

bool
memory_backend::write_all(std::string_view relative_path, std::span<const uint8_t> data)
{
    std::string key(relative_path);

    auto& entry = m_files[key];
    entry.data.assign(data.begin(), data.end());
    entry.last_modified = std::filesystem::file_time_type::clock::now();
    return true;
}

bool
memory_backend::create_directories(std::string_view relative_path)
{
    m_directories.insert(std::string(relative_path));
    return true;
}

bool
memory_backend::remove(std::string_view relative_path)
{
    std::string prefix(relative_path);

    m_directories.erase(prefix);

    auto it = m_files.begin();
    while (it != m_files.end())
    {
        if (it->first == prefix || it->first.starts_with(prefix + "/"))
        {
            it = m_files.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return true;
}

bool
memory_backend::enumerate(std::string_view relative_path,
                          const enumerate_cb& visitor,
                          bool recursive,
                          std::string_view ext_filter) const
{
    std::string prefix(relative_path);
    if (!prefix.empty() && prefix.back() != '/')
        prefix += '/';

    for (auto& [path, entry] : m_files)
    {
        if (!path.starts_with(prefix) && !relative_path.empty())
            continue;

        if (relative_path.empty() || path.starts_with(prefix))
        {
            if (!recursive)
            {
                auto rest = std::string_view(path).substr(prefix.size());
                if (rest.find('/') != std::string_view::npos)
                    continue;
            }

            if (!ext_filter.empty())
            {
                auto dot = path.rfind('.');
                if (dot == std::string::npos)
                    continue;

                auto ext = std::string_view(path).substr(dot);
                if (ext != ext_filter)
                    continue;
            }

            if (!visitor(path, false))
                return false;
        }
    }

    return true;
}

std::optional<std::filesystem::path>
memory_backend::real_path(std::string_view) const
{
    return std::nullopt;
}

void
memory_backend::add_file(std::string path, std::vector<uint8_t> data)
{
    file_entry entry;
    entry.data = std::move(data);
    entry.last_modified = std::filesystem::file_time_type::clock::now();
    m_files[std::move(path)] = std::move(entry);
}

void
memory_backend::add_file_string(std::string path, std::string_view content)
{
    std::vector<uint8_t> data(content.begin(), content.end());
    add_file(std::move(path), std::move(data));
}

}  // namespace vfs
}  // namespace kryga
