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
    // Bind the committed base + local session-delta locations. Set once; load()
    // and save() operate on these so callers don't repeat the rid pair.
    void
    bind(const vfs::rid& base, const vfs::rid& cache);

    // Overlay: apply only the keys present in `config` onto the current values
    // (missing keys keep their current/default value).
    void
    load(const vfs::rid& config);

    // Layered load: committed base, then overlay the local session delta if it
    // exists. Runtime always works with the resulting (session) value.
    bool
    load();

    // Persist the session delta: write to the bound cache only the keys whose
    // current (session) value differs from the committed base. No-op if unbound.
    bool
    save() const;

    vfs::rid m_base_rid;
    vfs::rid m_cache_rid;

    bool force_recompile_shaders = false;
    uint32_t fps_lock = 30;
    utils::id level;
    uint32_t window_w = 1600;
    uint32_t window_h = 900;
    // Number of per-instance render-object slots preallocated at startup (handles
    // + render_cache storage). A floor, not a cap — usage grows past it. Sizes the
    // object SSBO and cull dispatch, so keep it near the real scene budget.
    uint32_t object_pool_size = 4096;
};
}  // namespace editor
}  // namespace kryga
