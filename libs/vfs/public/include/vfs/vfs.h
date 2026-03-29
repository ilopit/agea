#pragma once

#include "vfs/backend.h"
#include "vfs/rid.h"
#include "vfs/vfs_types.h"

#include <string>
#include <string_view>
#include <vector>

namespace kryga
{

namespace vfs
{

class virtual_file_system
{
public:
    virtual_file_system() = default;
    ~virtual_file_system() = default;

    virtual_file_system(const virtual_file_system&) = delete;
    virtual_file_system&
    operator=(const virtual_file_system&) = delete;

    void
    mount(std::string mount_point, std::unique_ptr<backend> b, int priority);

    void
    unmount(std::string_view mount_point, backend* b);

    void
    set_write_target(std::string_view mount_point, backend* b);

    // Core I/O
    bool
    read_bytes(const rid& id, std::vector<uint8_t>& out) const;

    bool
    read_string(const rid& id, std::string& out) const;

    bool
    write_bytes(const rid& id, std::span<const uint8_t> data);

    bool
    write_string(const rid& id, std::string_view data);

    // Query
    file_info
    stat(const rid& id) const;

    bool
    exists(const rid& id) const;

    // Directory
    bool
    create_directories(const rid& id);

    bool
    remove(const rid& id);

    bool
    enumerate(const rid& id,
              const backend::enumerate_cb& visitor,
              bool recursive,
              std::string_view ext_filter = {}) const;

    // Escape hatch for external tools
    std::optional<std::filesystem::path>
    real_path(const rid& id) const;

    // Temp directory (RAII)
    temp_dir_context
    create_temp_dir();

private:
    struct mount_entry
    {
        std::string mount_point;
        std::unique_ptr<backend> be;
        int priority = 0;
    };

    backend*
    find_read_backend(std::string_view mount_point, std::string_view relative) const;

    backend*
    find_write_backend(std::string_view mount_point) const;

    std::vector<mount_entry> m_mounts;
    std::unordered_map<std::string, backend*> m_write_targets;
};

}  // namespace vfs
}  // namespace kryga
