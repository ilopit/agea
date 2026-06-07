#include "vulkan_render/render_config.h"

#include <global_state/global_state.h>
#include <serialization/serialization.h>
#include <vfs/vfs.h>
#include <gpu_types/gpu_shadow_types.h>
#include <gpu_types/gpu_cluster_types.h>
#include <utils/kryga_log.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace kryga
{
namespace render
{

namespace
{

// ============================================================================
// Enum <-> string tables
// ============================================================================

const std::unordered_map<std::string, pcf_mode> pcf_mode_from_string = {
    {"3x3", pcf_mode::pcf_3x3},
    {"5x5", pcf_mode::pcf_5x5},
    {"7x7", pcf_mode::pcf_7x7},
    {"poisson16", pcf_mode::poisson16},
    {"poisson32", pcf_mode::poisson32},
    {"poisson64", pcf_mode::poisson64},
    {"pcss", pcf_mode::pcss},
};

const std::unordered_map<pcf_mode, std::string> pcf_mode_to_string = {
    {pcf_mode::pcf_3x3, "3x3"},
    {pcf_mode::pcf_5x5, "5x5"},
    {pcf_mode::pcf_7x7, "7x7"},
    {pcf_mode::poisson16, "poisson16"},
    {pcf_mode::poisson32, "poisson32"},
    {pcf_mode::poisson64, "poisson64"},
    {pcf_mode::pcss, "pcss"},
};

const std::unordered_map<std::string, present_mode> present_mode_from_str = {
    {"fifo", present_mode::fifo},
    {"mailbox", present_mode::mailbox},
    {"immediate", present_mode::immediate},
};

const std::unordered_map<present_mode, std::string> present_mode_to_str = {
    {present_mode::fifo, "fifo"},
    {present_mode::mailbox, "mailbox"},
    {present_mode::immediate, "immediate"},
};

// ============================================================================
// Extract helpers
// ============================================================================

template <typename T>
void
extract_field(const YAML::Node& node, const char* key, T& field)
{
    auto v = node[key];
    if (!v || !v.IsScalar())
    {
        return;
    }
    field = v.as<T>();
}

void
extract_field(const YAML::Node& node, const char* key, pcf_mode& field)
{
    auto v = node[key];
    if (!v || !v.IsScalar())
    {
        return;
    }
    auto it = pcf_mode_from_string.find(v.as<std::string>());
    if (it != pcf_mode_from_string.end())
    {
        field = it->second;
    }
    else
    {
        ALOG_WARN("Unknown pcf mode '{}', keeping default", v.as<std::string>());
    }
}

void
extract_field(const YAML::Node& node, const char* key, bool& field)
{
    auto v = node[key];
    if (!v || !v.IsScalar())
    {
        return;
    }
    field = v.as<bool>();
}

void
extract_field(const YAML::Node& node, const char* key, present_mode& field)
{
    auto v = node[key];
    if (!v || !v.IsScalar())
    {
        return;
    }
    auto it = present_mode_from_str.find(v.as<std::string>());
    if (it != present_mode_from_str.end())
    {
        field = it->second;
    }
    else
    {
        ALOG_WARN("Unknown present mode '{}', keeping default", v.as<std::string>());
    }
}

}  // namespace

// ============================================================================
// Shadow atlas limit queries (used by validate, UI, RPC)
// ============================================================================

uint32_t
render_config::shadow_cfg::max_cascades() const
{
    if (csm_tile_size == 0)
    {
        return KGPU_CSM_CASCADE_COUNT_MIN;
    }
    uint32_t by_width = atlas_size / csm_tile_size;
    return std::clamp(
        by_width, (uint32_t)KGPU_CSM_CASCADE_COUNT_MIN, (uint32_t)KGPU_CSM_CASCADE_COUNT_MAX);
}

uint32_t
render_config::shadow_cfg::max_csm_tile() const
{
    if (cascade_count == 0)
    {
        return KGPU_SHADOW_MAP_SIZE_MIN;
    }
    uint32_t max_by_width = atlas_size / std::max(cascade_count, 1u);
    uint32_t max_by_height = atlas_size - KGPU_SHADOW_MAP_SIZE_MIN;
    uint32_t limit = std::min(max_by_width, max_by_height);
    // Round down to power of two
    uint32_t p = 1;
    while (p * 2 <= limit)
    {
        p *= 2;
    }
    return std::max(p, (uint32_t)KGPU_SHADOW_MAP_SIZE_MIN);
}

uint32_t
render_config::shadow_cfg::max_local_tile() const
{
    const uint32_t local_tile_count = max_local_lights * 2;  // front+back hemisphere per light
    if (local_tile_count == 0 || atlas_size <= csm_tile_size)
    {
        return KGPU_SHADOW_MAP_SIZE_MIN;
    }
    // Local tiles are laid out in a grid below the CSM row (see
    // compute_shadow_atlas_layout): cols = atlas/size, rows = ceil(count/cols), and the
    // grid bottom sits at csm_tile + rows*size. Return the largest power-of-two tile size
    // whose FULL grid fits in the atlas — the previous version only checked that a single
    // row fit the vertical remainder, so larger sizes (fewer cols => more rows) overflowed.
    uint32_t best = 0;
    for (uint32_t s = KGPU_SHADOW_MAP_SIZE_MIN; s <= atlas_size; s *= 2)
    {
        uint32_t cols = atlas_size / s;
        if (cols == 0)
        {
            break;
        }
        uint32_t rows = (local_tile_count + cols - 1) / cols;  // ceil
        if (csm_tile_size + rows * s <= atlas_size)
        {
            best = s;
        }
    }
    return std::max(best, (uint32_t)KGPU_SHADOW_MAP_SIZE_MIN);
}

// ============================================================================
// Validate
// ============================================================================

void
render_config::validate()
{
    // Helper: clamp and warn if value was out of range
    auto clamp_warn = [](auto& value, auto lo, auto hi, const char* name)
    {
        auto original = value;
        value = std::clamp(value, lo, hi);
        if (value != original)
        {
            ALOG_WARN("render_config: '{}' value {} clamped to [{}, {}]", name, original, lo, hi);
        }
    };

    // Shadows — pcf mode
    auto pcf_val = static_cast<uint32_t>(shadows.pcf);
    clamp_warn(pcf_val,
               static_cast<uint32_t>(pcf_mode::min),
               static_cast<uint32_t>(pcf_mode::max),
               "shadows.pcf");
    shadows.pcf = static_cast<pcf_mode>(pcf_val);

    clamp_warn(shadows.bias, KGPU_SHADOW_BIAS_MIN, KGPU_SHADOW_BIAS_MAX, "shadows.bias");
    clamp_warn(shadows.normal_bias,
               KGPU_SHADOW_NORMAL_BIAS_MIN,
               KGPU_SHADOW_NORMAL_BIAS_MAX,
               "shadows.normal_bias");
    clamp_warn(
        shadows.local_bias, KGPU_SHADOW_BIAS_MIN, KGPU_SHADOW_BIAS_MAX, "shadows.local_bias");
    clamp_warn(shadows.local_normal_bias,
               KGPU_SHADOW_NORMAL_BIAS_MIN,
               KGPU_SHADOW_NORMAL_BIAS_MAX,
               "shadows.local_normal_bias");
    clamp_warn(shadows.pcss_light_size,
               KGPU_PCSS_LIGHT_SIZE_MIN,
               KGPU_PCSS_LIGHT_SIZE_MAX,
               "shadows.pcss_light_size");
    clamp_warn(shadows.pcss_bias, KGPU_SHADOW_BIAS_MIN, KGPU_SHADOW_BIAS_MAX, "shadows.pcss_bias");
    clamp_warn(shadows.pcss_normal_bias,
               KGPU_SHADOW_NORMAL_BIAS_MIN,
               KGPU_SHADOW_NORMAL_BIAS_MAX,
               "shadows.pcss_normal_bias");
    clamp_warn(shadows.cascade_count,
               (uint32_t)KGPU_CSM_CASCADE_COUNT_MIN,
               (uint32_t)KGPU_CSM_CASCADE_COUNT_MAX,
               "shadows.cascade_count");
    clamp_warn(shadows.cascade_split_lambda, 0.0f, 1.0f, "shadows.cascade_split_lambda");
    clamp_warn(
        shadows.distance, KGPU_SHADOW_DISTANCE_MIN, KGPU_SHADOW_DISTANCE_MAX, "shadows.distance");
    // Round a value to the nearest power of two within [lo, hi]
    auto round_pow2 = [](uint32_t& value, uint32_t lo, uint32_t hi, const char* name)
    {
        value = std::clamp(value, lo, hi);
        if (value & (value - 1))
        {
            uint32_t original = value;
            uint32_t v = value;
            v--;
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
            v++;
            uint32_t lower = v >> 1;
            value = (v - original <= original - lower) ? v : lower;
            value = std::clamp(value, lo, hi);
            ALOG_WARN("render_config: '{}' value {} is not a power of two, rounded to {}",
                      name,
                      original,
                      value);
        }
    };

    round_pow2(shadows.atlas_size, 1024u, 16384u, "shadows.atlas_size");
    round_pow2(shadows.csm_tile_size,
               (uint32_t)KGPU_SHADOW_MAP_SIZE_MIN,
               shadows.atlas_size,
               "shadows.csm_tile_size");
    round_pow2(shadows.local_tile_size,
               (uint32_t)KGPU_SHADOW_MAP_SIZE_MIN,
               shadows.atlas_size,
               "shadows.local_tile_size");

    clamp_warn(shadows.max_local_lights,
               0u,
               (uint32_t)KGPU_MAX_SHADOWED_LOCAL_LIGHTS,
               "shadows.max_local_lights");

    // Priority: atlas > csm_tile > cascade_count > local_tile.
    // Shrink CSM tile first to preserve cascade count; only drop cascades
    // if CSM tile hits the minimum and still doesn't fit.
    while (shadows.csm_tile_size > shadows.max_csm_tile() &&
           shadows.csm_tile_size > KGPU_SHADOW_MAP_SIZE_MIN)
    {
        shadows.csm_tile_size /= 2;
    }
    clamp_warn(shadows.cascade_count, 1u, shadows.max_cascades(), "shadows.cascade_count");
    while (shadows.local_tile_size > shadows.max_local_tile() &&
           shadows.local_tile_size > KGPU_SHADOW_MAP_SIZE_MIN)
    {
        shadows.local_tile_size /= 2;
    }

    // Clusters
    clamp_warn(clusters.tile_size,
               (uint32_t)KGPU_cluster_tile_size_min,
               (uint32_t)KGPU_cluster_tile_size_max,
               "clusters.tile_size");
    clamp_warn(clusters.depth_slices,
               (uint32_t)KGPU_cluster_depth_slices_min,
               (uint32_t)KGPU_cluster_depth_slices_max,
               "clusters.depth_slices");
    clamp_warn(clusters.max_lights_per_cluster,
               (uint32_t)KGPU_max_lights_per_cluster_min,
               (uint32_t)KGPU_max_lights_per_cluster_max,
               "clusters.max_lights_per_cluster");

    // Render-scale — divisor must be >= 1; cap at 10 to avoid nonsensical values.
    clamp_warn(render_scale.divisor, 1u, 10u, "render_scale.divisor");

    // Outline thresholds are unbounded in principle, but clamp to a sane range
    // so a malformed config doesn't produce a solid-color screen.
    clamp_warn(outline.depth_threshold, 0.0f, 10.0f, "outline.depth_threshold");
    clamp_warn(outline.normal_threshold, 0.0f, 1.0f, "outline.normal_threshold");

    // frames_in_flight: coarse clamp (no swapchain known at config time); the
    // device clamps again to the surface's supported image count when applying.
    clamp_warn(frames_in_flight, 1u, 4u, "frames_in_flight");

    // present_mode: RPC can hand us an arbitrary int; pin to a known value so
    // present_mode_to_str.at() (in save) never throws on a stray enum.
    if (present != present_mode::fifo && present != present_mode::mailbox &&
        present != present_mode::immediate)
    {
        ALOG_WARN("render_config: unknown present_mode {}, defaulting to fifo",
                  static_cast<uint32_t>(present));
        present = present_mode::fifo;
    }

    // present_pace_frames lives INSIDE the frames_in_flight budget: pacing to
    // more frames than the queue can ever hold is a no-op (acquire blocks first).
    // frames_in_flight has priority — clamp pace DOWN to it rather than raising
    // the buffer. Clamped after frames_in_flight above, so the ceiling is final.
    // 0 = off.
    clamp_warn(present_pace_frames, 0u, frames_in_flight, "present_pace_frames");
}

// ============================================================================
// Load
// ============================================================================

bool
render_config::load(const vfs::rid& rid)
{
    serialization::container container;
    if (!serialization::read_container(rid, container))
    {
        ALOG_WARN("Failed to load render config from '{}', using defaults", rid.str());
        return false;
    }

    if (auto shadows_node = container["shadows"]; shadows_node && shadows_node.IsMap())
    {
        extract_field(shadows_node, "enabled", shadows.enabled);
        extract_field(shadows_node, "pcf", shadows.pcf);
        extract_field(shadows_node, "bias", shadows.bias);
        extract_field(shadows_node, "normal_bias", shadows.normal_bias);
        extract_field(shadows_node, "local_bias", shadows.local_bias);
        extract_field(shadows_node, "local_normal_bias", shadows.local_normal_bias);
        extract_field(shadows_node, "pcf_world_radius", shadows.pcf_world_radius);
        extract_field(shadows_node, "pcss_light_size", shadows.pcss_light_size);
        extract_field(shadows_node, "pcss_bias", shadows.pcss_bias);
        extract_field(shadows_node, "pcss_normal_bias", shadows.pcss_normal_bias);
        extract_field(shadows_node, "hardware_pcf", shadows.hardware_pcf);
        extract_field(shadows_node, "hardware_pcf_local", shadows.hardware_pcf_local);
        extract_field(shadows_node, "depth_16bit", shadows.depth_16bit);
        extract_field(shadows_node, "cascade_count", shadows.cascade_count);
        extract_field(shadows_node, "cascade_split_lambda", shadows.cascade_split_lambda);
        extract_field(shadows_node, "distance", shadows.distance);
        extract_field(shadows_node, "atlas_size", shadows.atlas_size);
        extract_field(shadows_node, "csm_tile_size", shadows.csm_tile_size);
        extract_field(shadows_node, "local_tile_size", shadows.local_tile_size);
        extract_field(shadows_node, "max_local_lights", shadows.max_local_lights);

        // Backward compat: old configs had map_size instead of per-tile sizes
        if (!shadows_node["csm_tile_size"] && shadows_node["map_size"])
        {
            shadows.csm_tile_size = shadows_node["map_size"].as<uint32_t>();
            shadows.local_tile_size = shadows.csm_tile_size / 2;
        }
    }

    if (auto clusters_node = container["clusters"]; clusters_node && clusters_node.IsMap())
    {
        extract_field(clusters_node, "tile_size", clusters.tile_size);
        extract_field(clusters_node, "depth_slices", clusters.depth_slices);
        extract_field(clusters_node, "max_lights_per_cluster", clusters.max_lights_per_cluster);
    }

    if (auto lighting_node = container["lighting"]; lighting_node && lighting_node.IsMap())
    {
        extract_field(lighting_node, "directional", lighting.directional_enabled);
        extract_field(lighting_node, "local", lighting.local_enabled);
        extract_field(lighting_node, "baked", lighting.baked_enabled);
    }

    if (auto debug_node = container["debug"]; debug_node && debug_node.IsMap())
    {
        extract_field(debug_node, "editor_mode", debug.editor_mode);
        extract_field(debug_node, "show_grid", debug.show_grid);
        extract_field(debug_node, "light_wireframe", debug.light_wireframe);
        extract_field(debug_node, "light_icons", debug.light_icons);
        extract_field(debug_node, "frustum_culling", debug.frustum_culling);
    }

    if (auto rs_node = container["render_scale"]; rs_node && rs_node.IsMap())
    {
        extract_field(rs_node, "enabled", render_scale.enabled);
        extract_field(rs_node, "divisor", render_scale.divisor);
    }

    if (auto ol_node = container["outline"]; ol_node && ol_node.IsMap())
    {
        extract_field(ol_node, "enabled", outline.enabled);
        extract_field(ol_node, "depth_threshold", outline.depth_threshold);
        extract_field(ol_node, "normal_threshold", outline.normal_threshold);
    }

    extract_field(container, "frames_in_flight", frames_in_flight);
    extract_field(container, "present_mode", present);
    extract_field(container, "present_pace_frames", present_pace_frames);

    validate();

    ALOG_INFO("Loaded render config from '{}'", rid.str());
    return true;
}

// ============================================================================
// Save
// ============================================================================

bool
render_config::save(const utils::path& path) const
{
    YAML::Node root;

    YAML::Node shadows_node;
    shadows_node["enabled"] = shadows.enabled;
    shadows_node["pcf"] = pcf_mode_to_string.at(shadows.pcf);
    shadows_node["bias"] = shadows.bias;
    shadows_node["normal_bias"] = shadows.normal_bias;
    shadows_node["local_bias"] = shadows.local_bias;
    shadows_node["local_normal_bias"] = shadows.local_normal_bias;
    shadows_node["pcf_world_radius"] = shadows.pcf_world_radius;
    shadows_node["pcss_light_size"] = shadows.pcss_light_size;
    shadows_node["pcss_bias"] = shadows.pcss_bias;
    shadows_node["pcss_normal_bias"] = shadows.pcss_normal_bias;
    shadows_node["hardware_pcf"] = shadows.hardware_pcf;
    shadows_node["hardware_pcf_local"] = shadows.hardware_pcf_local;
    shadows_node["depth_16bit"] = shadows.depth_16bit;
    shadows_node["cascade_count"] = shadows.cascade_count;
    shadows_node["cascade_split_lambda"] = shadows.cascade_split_lambda;
    shadows_node["distance"] = shadows.distance;
    shadows_node["atlas_size"] = shadows.atlas_size;
    shadows_node["csm_tile_size"] = shadows.csm_tile_size;
    shadows_node["local_tile_size"] = shadows.local_tile_size;
    shadows_node["max_local_lights"] = shadows.max_local_lights;
    root["shadows"] = shadows_node;

    YAML::Node clusters_node;
    clusters_node["tile_size"] = clusters.tile_size;
    clusters_node["depth_slices"] = clusters.depth_slices;
    clusters_node["max_lights_per_cluster"] = clusters.max_lights_per_cluster;
    root["clusters"] = clusters_node;

    YAML::Node lighting_node;
    lighting_node["directional"] = lighting.directional_enabled;
    lighting_node["local"] = lighting.local_enabled;
    lighting_node["baked"] = lighting.baked_enabled;
    root["lighting"] = lighting_node;

    YAML::Node debug_node;
    debug_node["editor_mode"] = debug.editor_mode;
    debug_node["show_grid"] = debug.show_grid;
    debug_node["light_wireframe"] = debug.light_wireframe;
    debug_node["light_icons"] = debug.light_icons;
    debug_node["frustum_culling"] = debug.frustum_culling;
    root["debug"] = debug_node;

    YAML::Node rs_node;
    rs_node["enabled"] = render_scale.enabled;
    rs_node["divisor"] = render_scale.divisor;
    root["render_scale"] = rs_node;

    YAML::Node ol_node;
    ol_node["enabled"] = outline.enabled;
    ol_node["depth_threshold"] = outline.depth_threshold;
    ol_node["normal_threshold"] = outline.normal_threshold;
    root["outline"] = ol_node;

    root["frames_in_flight"] = frames_in_flight;
    root["present_mode"] = present_mode_to_str.at(present);
    root["present_pace_frames"] = present_pace_frames;

    if (!serialization::write_container(path, root))
    {
        ALOG_ERROR("Failed to save render config to '{}'", path.str());
        return false;
    }

    ALOG_INFO("Saved render config to '{}'", path.str());
    return true;
}

// ============================================================================
// Cache support (via VFS)
// ============================================================================

void
render_config::bind(const vfs::rid& base, const vfs::rid& cache)
{
    m_base_rid = base;
    m_cache_rid = cache;
}

bool
render_config::load()
{
    // Base layer: committed defaults (read via VFS so APK-asset backends work).
    load(m_base_rid);

    // Session layer: overlay the local delta when present. load() is a per-key
    // overlay, so missing delta keys keep their base value.
    auto& vfs = glob::glob_state().getr_vfs();
    if (vfs.exists(m_cache_rid))
    {
        ALOG_INFO("Overlaying render config delta from '{}'", m_cache_rid.str());
        load(m_cache_rid);
    }

    validate();
    return true;
}

bool
render_config::save() const
{
    if (m_cache_rid.empty())
    {
        return false;
    }

    // Diff against a freshly-loaded base; persist only what the session changed.
    render_config base_cfg;
    base_cfg.load(m_base_rid);

    glob::glob_state().getr_vfs().create_directories(vfs::rid(m_cache_rid.mount_point(), ""));

    YAML::Node root;

// Emit `key` into `node` only when the session value differs from base.
#define DELTA(node, key, expr) \
    if ((expr) != (base_cfg.expr)) node[key] = (expr)

    YAML::Node shadows_node;
    DELTA(shadows_node, "enabled", shadows.enabled);
    if (shadows.pcf != base_cfg.shadows.pcf)
        shadows_node["pcf"] = pcf_mode_to_string.at(shadows.pcf);
    DELTA(shadows_node, "bias", shadows.bias);
    DELTA(shadows_node, "normal_bias", shadows.normal_bias);
    DELTA(shadows_node, "local_bias", shadows.local_bias);
    DELTA(shadows_node, "local_normal_bias", shadows.local_normal_bias);
    DELTA(shadows_node, "pcf_world_radius", shadows.pcf_world_radius);
    DELTA(shadows_node, "pcss_light_size", shadows.pcss_light_size);
    DELTA(shadows_node, "pcss_bias", shadows.pcss_bias);
    DELTA(shadows_node, "pcss_normal_bias", shadows.pcss_normal_bias);
    DELTA(shadows_node, "hardware_pcf", shadows.hardware_pcf);
    DELTA(shadows_node, "hardware_pcf_local", shadows.hardware_pcf_local);
    DELTA(shadows_node, "depth_16bit", shadows.depth_16bit);
    DELTA(shadows_node, "cascade_count", shadows.cascade_count);
    DELTA(shadows_node, "cascade_split_lambda", shadows.cascade_split_lambda);
    DELTA(shadows_node, "distance", shadows.distance);
    DELTA(shadows_node, "atlas_size", shadows.atlas_size);
    DELTA(shadows_node, "csm_tile_size", shadows.csm_tile_size);
    DELTA(shadows_node, "local_tile_size", shadows.local_tile_size);
    DELTA(shadows_node, "max_local_lights", shadows.max_local_lights);
    if (shadows_node.size() > 0)
        root["shadows"] = shadows_node;

    YAML::Node clusters_node;
    DELTA(clusters_node, "tile_size", clusters.tile_size);
    DELTA(clusters_node, "depth_slices", clusters.depth_slices);
    DELTA(clusters_node, "max_lights_per_cluster", clusters.max_lights_per_cluster);
    if (clusters_node.size() > 0)
        root["clusters"] = clusters_node;

    YAML::Node lighting_node;
    DELTA(lighting_node, "directional", lighting.directional_enabled);
    DELTA(lighting_node, "local", lighting.local_enabled);
    DELTA(lighting_node, "baked", lighting.baked_enabled);
    if (lighting_node.size() > 0)
        root["lighting"] = lighting_node;

    YAML::Node debug_node;
    DELTA(debug_node, "editor_mode", debug.editor_mode);
    DELTA(debug_node, "show_grid", debug.show_grid);
    DELTA(debug_node, "light_wireframe", debug.light_wireframe);
    DELTA(debug_node, "light_icons", debug.light_icons);
    DELTA(debug_node, "frustum_culling", debug.frustum_culling);
    if (debug_node.size() > 0)
        root["debug"] = debug_node;

    YAML::Node rs_node;
    DELTA(rs_node, "enabled", render_scale.enabled);
    DELTA(rs_node, "divisor", render_scale.divisor);
    if (rs_node.size() > 0)
        root["render_scale"] = rs_node;

    YAML::Node ol_node;
    DELTA(ol_node, "enabled", outline.enabled);
    DELTA(ol_node, "depth_threshold", outline.depth_threshold);
    DELTA(ol_node, "normal_threshold", outline.normal_threshold);
    if (ol_node.size() > 0)
        root["outline"] = ol_node;

    DELTA(root, "frames_in_flight", frames_in_flight);
    if (present != base_cfg.present)
        root["present_mode"] = present_mode_to_str.at(present);
    DELTA(root, "present_pace_frames", present_pace_frames);

#undef DELTA

    return serialization::write_container(m_cache_rid, root);
}

}  // namespace render
}  // namespace kryga
