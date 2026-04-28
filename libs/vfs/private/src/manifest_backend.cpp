#include "vfs/manifest_backend.h"

namespace kryga
{
namespace vfs
{

manifest_backend::manifest_backend()
    : backend(/*read_only=*/true)
{
}

std::string_view
manifest_backend::name() const
{
    return "manifest";
}

file_info
manifest_backend::stat(std::string_view /*relative_path*/) const
{
    // Never claims to hold files — reads fall through to the parent mount.
    return {};
}

bool
manifest_backend::read_all(std::string_view /*relative_path*/,
                           std::vector<uint8_t>& /*out*/) const
{
    return false;
}

bool
manifest_backend::write_all(std::string_view /*relative_path*/,
                            std::span<const uint8_t> /*data*/)
{
    return false;
}

bool
manifest_backend::create_directories(std::string_view /*relative_path*/)
{
    return false;
}

bool
manifest_backend::remove(std::string_view /*relative_path*/)
{
    return false;
}

bool
manifest_backend::enumerate(std::string_view /*relative_path*/,
                            const enumerate_cb& /*visitor*/,
                            bool /*recursive*/,
                            std::string_view /*ext_filter*/) const
{
    return true;
}

std::optional<std::filesystem::path>
manifest_backend::real_path(std::string_view /*relative_path*/) const
{
    return std::nullopt;
}

}  // namespace vfs
}  // namespace kryga
