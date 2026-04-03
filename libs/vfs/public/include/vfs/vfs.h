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

    struct mount_config
    {
        int priority = 0;
        std::string_view index_filter;               // if non-empty, build_index after mount
        std::vector<std::string> load_order;          // path prefixes for ordered iteration
    };

    // Mount a physical directory at a scoped path (e.g. rid("data", "packages/base.apkg")).
    // The relative part must be non-empty. Returns backend pointer (owned by VFS).
    backend*
    mount(const rid& target, std::filesystem::path root, const mount_config& cfg = {});

    // Mount a pre-built backend (for tests, memory backends, etc.)
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

    // Find an object by name using backend indexes.
    // Searches backends matching the scope (mount_point + relative prefix).
    // If scope has empty relative, searches all backends at that mount point.
    std::optional<rid>
    find_object(const rid& scope, std::string_view name) const;

    // Iterate indexed objects for a specific backend, or merge all at scope.
    using object_visitor = std::function<bool(std::string_view name, const rid& path)>;
    void
    enumerate_objects(const rid& scope,
                      const object_visitor& visitor,
                      backend* be = nullptr) const;

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
        std::string scope;  // relative subpath (empty for root mounts)
        std::unique_ptr<backend> be;
        int priority = 0;
    };

    struct resolved_backend
    {
        backend* be = nullptr;
        std::string relative;  // path relative to the backend (scope stripped)
    };

    resolved_backend
    find_read_backend(std::string_view mount_point, std::string_view relative) const;

    backend*
    find_write_backend(std::string_view mount_point) const;

    std::vector<mount_entry> m_mounts;
    std::unordered_map<std::string, backend*> m_write_targets;
};

}  // namespace vfs
}  // namespace kryga
