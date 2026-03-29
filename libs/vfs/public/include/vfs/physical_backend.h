#pragma once

#include "vfs/backend.h"

#include <filesystem>

namespace kryga
{
namespace vfs
{

class physical_backend : public backend
{
public:
    explicit physical_backend(std::filesystem::path root);

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
    std::filesystem::path
    resolve(std::string_view relative_path) const;

    std::filesystem::path m_root;
};

}  // namespace vfs
}  // namespace kryga
