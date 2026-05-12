#if defined(__ANDROID__)

#include "vfs/android_asset_backend.h"

#include <utils/kryga_log.h>

#include <android/asset_manager.h>

namespace kryga
{
namespace vfs
{

android_asset_backend::android_asset_backend(AAssetManager* mgr, std::string root_prefix)
    : backend(/*read_only=*/true)
    , m_mgr(mgr)
    , m_root_prefix(std::move(root_prefix))
{
    if (!m_root_prefix.empty() && m_root_prefix.back() != '/')
    {
        m_root_prefix.push_back('/');
    }
}

std::string_view
android_asset_backend::name() const
{
    return "android_asset";
}

std::string
android_asset_backend::resolve(std::string_view relative_path) const
{
    std::string out;
    out.reserve(m_root_prefix.size() + relative_path.size());
    out.append(m_root_prefix);
    out.append(relative_path);
    return out;
}

file_info
android_asset_backend::stat(std::string_view relative_path) const
{
    auto full = resolve(relative_path);

    // Try to open as a file first.
    if (AAsset* a = AAssetManager_open(m_mgr, full.c_str(), AASSET_MODE_UNKNOWN))
    {
        file_info info;
        info.exists = true;
        info.is_directory = false;
        info.size = static_cast<uint64_t>(AAsset_getLength64(a));
        AAsset_close(a);
        return info;
    }

    // Fall back to directory probe.
    if (AAssetDir* d = AAssetManager_openDir(m_mgr, full.c_str()))
    {
        // AAssetManager_openDir succeeds even for non-existent paths, so we
        // must verify by asking for the first entry.
        const char* first = AAssetDir_getNextFileName(d);
        AAssetDir_close(d);
        if (first != nullptr)
        {
            file_info info;
            info.exists = true;
            info.is_directory = true;
            return info;
        }
    }

    return {};
}

bool
android_asset_backend::read_all(std::string_view relative_path, std::vector<uint8_t>& out) const
{
    auto full = resolve(relative_path);

    AAsset* a = AAssetManager_open(m_mgr, full.c_str(), AASSET_MODE_BUFFER);
    if (!a)
    {
        return false;
    }

    off64_t size = AAsset_getLength64(a);
    out.resize(static_cast<size_t>(size));

    int read = AAsset_read(a, out.data(), static_cast<size_t>(size));
    AAsset_close(a);

    return read == size;
}

bool
android_asset_backend::write_all(std::string_view, std::span<const uint8_t>)
{
    // APK assets are read-only.
    return false;
}

bool
android_asset_backend::create_directories(std::string_view)
{
    return false;
}

bool
android_asset_backend::remove(std::string_view)
{
    return false;
}

bool
android_asset_backend::enumerate(std::string_view relative_path,
                                 const enumerate_cb& visitor,
                                 bool recursive,
                                 std::string_view ext_filter) const
{
    auto full = resolve(relative_path);

    AAssetDir* d = AAssetManager_openDir(m_mgr, full.c_str());
    if (!d)
    {
        return true;
    }

    // Note: AAssetManager_openDir is NON-recursive and only enumerates files,
    // not subdirectories. Recursive enumeration is not supported by the NDK
    // API — callers needing recursion must pre-bake a file index at build
    // time and bundle it into the APK.
    if (recursive)
    {
        ALOG_WARN(
            "android_asset_backend::enumerate: recursive enumeration is not supported by "
            "AAssetManager; only top-level files will be visited for '{}'",
            full);
    }

    while (const char* fname = AAssetDir_getNextFileName(d))
    {
        std::string_view sv{fname};

        if (!ext_filter.empty())
        {
            // Match suffix.
            if (sv.size() < ext_filter.size())
            {
                continue;
            }
            auto tail = sv.substr(sv.size() - ext_filter.size());
            if (tail != ext_filter)
            {
                continue;
            }
        }

        if (!visitor(sv, /*is_directory=*/false))
        {
            AAssetDir_close(d);
            return false;
        }
    }

    AAssetDir_close(d);
    return true;
}

std::optional<std::filesystem::path>
android_asset_backend::real_path(std::string_view) const
{
    // Assets inside the APK have no filesystem path.
    return std::nullopt;
}

}  // namespace vfs
}  // namespace kryga

#endif  // __ANDROID__
