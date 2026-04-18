#pragma once

#include "vfs/backend.h"

#include <filesystem>
#include <string>

struct AAssetManager;

namespace kryga
{
namespace vfs
{

// Read-only backend backed by Android's AAssetManager, serving files bundled
// inside the APK's assets/ directory. Writes, create_directories, remove all
// return false. For writable storage on Android, use physical_backend pointing
// at the app's internal storage path.
class android_asset_backend : public backend
{
public:
    explicit android_asset_backend(AAssetManager* mgr, std::string root_prefix = {});

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

private:
    std::string
    resolve(std::string_view relative_path) const;

    AAssetManager* m_mgr = nullptr;
    std::string m_root_prefix;  // optional "subdir/" prefix inside assets/
};

}  // namespace vfs
}  // namespace kryga
