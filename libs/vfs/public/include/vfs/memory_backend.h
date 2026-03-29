#pragma once

#include "vfs/backend.h"

#include <unordered_map>
#include <unordered_set>

namespace kryga
{
namespace vfs
{

class memory_backend : public backend
{
public:
    memory_backend() = default;

    std::string_view
    name() const override;

    file_info
    stat(std::string_view relative_path) const override;

    bool
    read_all(std::string_view relative_path, std::vector<uint8_t>& out) const override;

    bool
    write_all(std::string_view relative_path, std::span<const uint8_t> data) override;

    bool
    create_directories(std::string_view relative_path) override;

    bool
    remove(std::string_view relative_path) override;

    bool
    enumerate(std::string_view relative_path,
              const enumerate_cb& visitor,
              bool recursive,
              std::string_view ext_filter = {}) const override;

    std::optional<std::filesystem::path>
    real_path(std::string_view relative_path) const override;

    void
    add_file(std::string path, std::vector<uint8_t> data);

    void
    add_file_string(std::string path, std::string_view content);

private:
    struct file_entry
    {
        std::vector<uint8_t> data;
        std::filesystem::file_time_type last_modified{};
    };

    std::unordered_map<std::string, file_entry> m_files;
    std::unordered_set<std::string> m_directories;
};

}  // namespace vfs
}  // namespace kryga
