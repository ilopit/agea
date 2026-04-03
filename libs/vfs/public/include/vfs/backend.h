#pragma once

#include "vfs/file_index.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kryga
{
namespace vfs
{

struct file_info
{
    uint64_t size = 0;
    std::filesystem::file_time_type last_modified{};
    bool exists = false;
    bool is_directory = false;
};

class backend
{
    friend class virtual_file_system;

public:
    explicit backend(bool read_only = false)
        : m_read_only(read_only)
    {
    }

    virtual ~backend() = default;

    virtual std::string_view
    name() const = 0;

    bool
    is_read_only() const
    {
        return m_read_only;
    }

    // Build index of files matching ext_filter. Uses enumerate() internally.
    bool
    build_index(std::string_view ext_filter = {});

    virtual file_info
    stat(std::string_view relative_path) const = 0;

    virtual bool
    read_all(std::string_view relative_path, std::vector<uint8_t>& out) const = 0;

    virtual bool
    write_all(std::string_view relative_path, std::span<const uint8_t> data) = 0;

    virtual bool
    create_directories(std::string_view relative_path) = 0;

    virtual bool
    remove(std::string_view relative_path) = 0;

    using enumerate_cb = std::function<bool(std::string_view path, bool is_directory)>;

    virtual bool
    enumerate(std::string_view relative_path,
              const enumerate_cb& visitor,
              bool recursive,
              std::string_view ext_filter = {}) const = 0;

    virtual std::optional<std::filesystem::path>
    real_path(std::string_view relative_path) const = 0;

private:
    bool m_read_only = false;
    file_index m_index;
};

}  // namespace vfs
}  // namespace kryga
