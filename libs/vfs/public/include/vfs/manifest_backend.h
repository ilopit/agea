#pragma once

#include "vfs/backend.h"

namespace kryga
{
namespace vfs
{

// A backend that exists only to carry a pre-baked `file_index` populated from
// a cooked `.kryga_index` manifest. All I/O methods return failure / empty —
// callers that read by rid fall through to the parent mount (e.g. the `data`
// backend) while enumerations walk this backend's index directly.
//
// Purpose: on Android APK assets, `physical_backend` can't be rooted at a
// level/package sub-tree (no filesystem path). This gives level/package
// loaders an indexable mount without needing real_path.
class manifest_backend : public backend
{
public:
    manifest_backend();

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
};

}  // namespace vfs
}  // namespace kryga
