#include "vfs/physical_backend.h"

#include <utils/kryga_log.h>

#include <fstream>

namespace kryga
{
namespace vfs
{

physical_backend::physical_backend(std::filesystem::path root)
    : m_root(std::move(root))
{
}

std::string_view
physical_backend::name() const
{
    return "physical";
}

std::filesystem::path
physical_backend::resolve(std::string_view relative_path) const
{
    if (relative_path.empty())
    {
        return m_root;
    }

    return m_root / relative_path;
}

file_info
physical_backend::stat(std::string_view relative_path) const
{
    auto full = resolve(relative_path);

    std::error_code ec;
    auto status = std::filesystem::status(full, ec);

    if (ec || !std::filesystem::exists(status))
    {
        return {};
    }

    file_info info;
    info.exists = true;
    info.is_directory = std::filesystem::is_directory(status);

    if (!info.is_directory)
    {
        info.size = std::filesystem::file_size(full, ec);
        if (ec)
        {
            info.size = 0;
        }
    }

    info.last_modified = std::filesystem::last_write_time(full, ec);

    return info;
}

bool
physical_backend::read_all(std::string_view relative_path, std::vector<uint8_t>& out) const
{
    auto full = resolve(relative_path);

    std::ifstream file(full, std::ios_base::binary | std::ios_base::ate);
    if (!file.is_open())
    {
        return false;
    }

    auto size = file.tellg();
    out.resize(static_cast<size_t>(size));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(out.data()), size);

    return true;
}

bool
physical_backend::write_all(std::string_view relative_path, std::span<const uint8_t> data)
{
    auto full = resolve(relative_path);

    std::error_code ec;
    std::filesystem::create_directories(full.parent_path(), ec);

    std::ofstream file(full, std::ios_base::binary);
    if (!file.is_open())
    {
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());

    return true;
}

bool
physical_backend::create_directories(std::string_view relative_path)
{
    auto full = resolve(relative_path);

    std::error_code ec;
    std::filesystem::create_directories(full, ec);

    return !ec;
}

bool
physical_backend::remove(std::string_view relative_path)
{
    auto full = resolve(relative_path);

    std::error_code ec;
    std::filesystem::remove_all(full, ec);

    return !ec;
}

bool
physical_backend::enumerate(std::string_view relative_path,
                            const enumerate_cb& visitor,
                            bool recursive,
                            std::string_view ext_filter) const
{
    auto full = resolve(relative_path);

    std::error_code ec;
    if (!std::filesystem::exists(full, ec))
    {
        return true;
    }

    auto iterate = [&](auto& iterator) -> bool
    {
        for (auto& entry : iterator)
        {
            if (entry.is_directory())
            {
                continue;
            }

            auto rel = std::filesystem::relative(entry.path(), full, ec).generic_string();
            if (ec)
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

            if (!visitor(rel, entry.is_directory()))
            {
                return false;
            }
        }
        return true;
    };

    if (recursive)
    {
        auto it = std::filesystem::recursive_directory_iterator(full, ec);
        if (ec)
        {
            return true;
        }
        return iterate(it);
    }
    else
    {
        auto it = std::filesystem::directory_iterator(full, ec);
        if (ec)
        {
            return true;
        }
        return iterate(it);
    }
}

std::optional<std::filesystem::path>
physical_backend::real_path(std::string_view relative_path) const
{
    return resolve(relative_path);
}

}  // namespace vfs
}  // namespace kryga
