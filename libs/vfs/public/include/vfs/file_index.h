#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace kryga
{
namespace vfs
{

class virtual_file_system;
class rid;

// Reusable file index — scans a directory tree and builds a filename-stem → relative-path map.
// Can be used for .aobj objects, shaders, textures, or any file type.
class file_index
{
public:
    file_index() = default;

    // Scan a physical directory. Only indexes files matching ext_filter (e.g. ".aobj").
    // Empty ext_filter indexes all files.
    // Returns false on duplicate stems or filesystem errors.
    bool
    build(const std::filesystem::path& root, std::string_view ext_filter = {});

    // Scan through VFS (sees overlays/mods).
    bool
    build(const virtual_file_system& vfs, const rid& root, std::string_view ext_filter = {});

    // Manual entry (tests, save, runtime additions).
    // Returns false if name already exists.
    bool
    add(std::string_view name, std::string_view relative_path);

    // Set prefix-based iteration order. Entries matching earlier prefixes come first.
    // Unmatched entries come last. Call after build/add, before iterating.
    void
    set_load_order(const std::vector<std::string>& prefixes);

    // Lookup by stem name. Returns false if not found.
    bool
    resolve(std::string_view name, std::string& out_relative) const;

    bool
    contains(std::string_view name) const;

    // Ordered iteration (respects load_order if set)
    const std::vector<std::pair<std::string, std::string>>&
    ordered_entries() const
    {
        return m_ordered;
    }

    // Unordered access (for merge operations)
    const std::unordered_map<std::string, std::string>&
    entries() const
    {
        return m_entries;
    }

    size_t
    size() const
    {
        return m_entries.size();
    }

    void
    clear();

private:
    void
    rebuild_ordered();

    // stem → relative_path (fast lookup)
    std::unordered_map<std::string, std::string> m_entries;
    // ordered iteration list
    std::vector<std::pair<std::string, std::string>> m_ordered;
    // prefix-based sort order
    std::vector<std::string> m_load_order;
};

}  // namespace vfs
}  // namespace kryga
