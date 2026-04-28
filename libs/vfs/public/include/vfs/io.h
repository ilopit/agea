#pragma once

#include <vfs/rid.h>
#include <utils/buffer.h>

#include <string>
#include <vector>

namespace kryga
{
namespace vfs
{

bool
load_buffer(const rid& id, utils::buffer& b);

bool
save_buffer(utils::buffer& b);

bool
load_file(const rid& id, std::vector<uint8_t>& blob);

bool
save_file(const rid& id, const std::vector<uint8_t>& blob);

// Load a cooked `kryga_index` manifest — one relative path per line, '#'
// starts a comment. Emitted by tools/cook for every `.apkg` and `.alvl`
// directory; replaces the filesystem-walking `build_index` step which
// doesn't work on Android APK assets. (Not dot-prefixed — AAPT filters
// hidden files out of APK packaging.)
struct index_entry
{
    std::string stem;      // filename without extension
    std::string relative;  // path relative to the manifest's directory
};

bool
load_index_manifest(const rid& manifest_id, std::vector<index_entry>& out);

}  // namespace vfs
}  // namespace kryga
