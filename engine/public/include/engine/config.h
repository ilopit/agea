#pragma once

#include <utils/path.h>
#include <utils/id.h>

#include <utils/check.h>

#include <vfs/rid.h>

namespace kryga
{
namespace editor
{
class config
{
public:
    void
    load(const vfs::rid& config);

    // Runtime overlay (mirrors render_config): read the writable rtcache copy if
    // present (session state — e.g. last opened level), else fall back to the
    // committed base config. The cache is local and never committed.
    bool
    load_with_cache(const vfs::rid& base, const vfs::rid& cache);

    // Persist the current config to the rtcache copy.
    bool
    save_to_cache(const vfs::rid& cache) const;

    bool force_recompile_shaders = false;
    uint32_t fps_lock = 30;
    utils::id level;
    uint32_t window_w = 1600;
    uint32_t window_h = 900;
};
}  // namespace editor
}  // namespace kryga
