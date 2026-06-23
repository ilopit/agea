#include "engine/config.h"

#include <global_state/global_state.h>
#include <utils/defines_utils.h>
#include <utils/kryga_log.h>
#include <serialization/serialization.h>
#include <vfs/vfs.h>

namespace kryga
{
void
state_mutator__config::set(gs::state& s)
{
    auto p = s.create_box<editor::config>("config");
    s.m_config = p;
}

namespace editor
{

namespace
{
template <typename T>
void
extract_field(kryga::serialization::container& c, const std::string& key, T& field)
{
    auto v = c[key];

    if (!v.IsScalar())
    {
        return;
    }

    if constexpr (std::is_same_v<T, ::kryga::utils::id>)
    {
        field = AID(v.as<std::string>());
    }
    else
    {
        field = v.as<T>();
    }
}
void
apply_fields(serialization::container& container, config& c)
{
    extract_field(container, KRG_stringify(force_recompile_shaders), c.force_recompile_shaders);
    extract_field(container, KRG_stringify(fps_lock), c.fps_lock);
    extract_field(container, KRG_stringify(level), c.level);
    extract_field(container, KRG_stringify(window_h), c.window_h);
    extract_field(container, KRG_stringify(window_w), c.window_w);
    extract_field(container, KRG_stringify(object_pool_size), c.object_pool_size);
}
}  // namespace

void
config::load(const vfs::rid& config_rid)
{
    serialization::container container;
    if (!serialization::read_container(config_rid, container))
    {
        return;
    }

    apply_fields(container, *this);
}

void
config::bind(const vfs::rid& base, const vfs::rid& cache)
{
    m_base_rid = base;
    m_cache_rid = cache;
}

bool
config::load()
{
    // Base layer: committed defaults.
    load(m_base_rid);

    // Session layer: overlay the local delta when present.
    auto& vfs = glob::glob_state().getr_vfs();
    if (vfs.exists(m_cache_rid))
    {
        ALOG_INFO("Overlaying engine config delta from '{}'", m_cache_rid.str());
        load(m_cache_rid);
    }
    return true;
}

bool
config::save() const
{
    if (m_cache_rid.empty())
    {
        return false;
    }

    // Diff against a freshly-loaded base; persist only what the session changed.
    config base_cfg;
    base_cfg.load(m_base_rid);

    glob::glob_state().getr_vfs().create_directories(vfs::rid(m_cache_rid.mount_point(), ""));

    YAML::Node root;
    if (force_recompile_shaders != base_cfg.force_recompile_shaders)
    {
        root[KRG_stringify(force_recompile_shaders)] = force_recompile_shaders;
    }
    if (fps_lock != base_cfg.fps_lock)
    {
        root[KRG_stringify(fps_lock)] = fps_lock;
    }
    if (level != base_cfg.level)
    {
        root[KRG_stringify(level)] = level.str();
    }
    if (window_h != base_cfg.window_h)
    {
        root[KRG_stringify(window_h)] = window_h;
    }
    if (window_w != base_cfg.window_w)
    {
        root[KRG_stringify(window_w)] = window_w;
    }
    if (object_pool_size != base_cfg.object_pool_size)
    {
        root[KRG_stringify(object_pool_size)] = object_pool_size;
    }

    return serialization::write_container(m_cache_rid, root);
}

}  // namespace editor
}  // namespace kryga
