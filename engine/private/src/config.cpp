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

    if constexpr (std::is_same<T, ::kryga::utils::id>::value)
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

bool
config::load_with_cache(const vfs::rid& base, const vfs::rid& cache)
{
    auto& vfs = glob::glob_state().getr_vfs();
    if (vfs.exists(cache))
    {
        serialization::container container;
        if (serialization::read_container(cache, container))
        {
            ALOG_INFO("Found cached engine config at '{}'", cache.str());
            apply_fields(container, *this);
            return true;
        }
    }

    // No session cache — fall back to the committed base config.
    load(base);
    return true;
}

bool
config::save_to_cache(const vfs::rid& cache) const
{
    glob::glob_state().getr_vfs().create_directories(vfs::rid(cache.mount_point(), ""));

    YAML::Node root;
    root[KRG_stringify(force_recompile_shaders)] = force_recompile_shaders;
    root[KRG_stringify(fps_lock)] = fps_lock;
    root[KRG_stringify(level)] = level.str();
    root[KRG_stringify(window_h)] = window_h;
    root[KRG_stringify(window_w)] = window_w;

    return serialization::write_container(cache, root);
}

}  // namespace editor
}  // namespace kryga
