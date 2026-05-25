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
};

const std::unordered_map<pcf_mode, std::string> pcf_mode_to_string = {
    {pcf_mode::pcf_3x3, "3x3"},
    {pcf_mode::pcf_5x5, "5x5"},
    {pcf_mode::pcf_7x7, "7x7"},
    {pcf_mode::poisson16, "poisson16"},
    {pcf_mode::poisson32, "poisson32"},
    {pcf_mode::poisson64, "poisson64"},
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

}  // namespace

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
    clamp_warn(shadows.cascade_count,
               (uint32_t)KGPU_CSM_CASCADE_COUNT_MIN,
               (uint32_t)KGPU_CSM_CASCADE_COUNT_MAX,
               "shadows.cascade_count");
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

    // CSM row must fit: 4 cascades side by side
    if (shadows.csm_tile_size * KGPU_CSM_CASCADE_COUNT > shadows.atlas_size)
    {
        shadows.csm_tile_size = shadows.atlas_size / KGPU_CSM_CASCADE_COUNT;
        ALOG_WARN("render_config: csm_tile_size reduced to {} to fit atlas", shadows.csm_tile_size);
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
        extract_field(shadows_node, "pcf_world_radius", shadows.pcf_world_radius);
        extract_field(shadows_node, "hardware_pcf", shadows.hardware_pcf);
        extract_field(shadows_node, "depth_16bit", shadows.depth_16bit);
        extract_field(shadows_node, "cascade_count", shadows.cascade_count);
        extract_field(shadows_node, "distance", shadows.distance);
        extract_field(shadows_node, "atlas_size", shadows.atlas_size);
        extract_field(shadows_node, "csm_tile_size", shadows.csm_tile_size);
        extract_field(shadows_node, "local_tile_size", shadows.local_tile_size);

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
    shadows_node["pcf_world_radius"] = shadows.pcf_world_radius;
    shadows_node["hardware_pcf"] = shadows.hardware_pcf;
    shadows_node["depth_16bit"] = shadows.depth_16bit;
    shadows_node["cascade_count"] = shadows.cascade_count;
    shadows_node["distance"] = shadows.distance;
    shadows_node["atlas_size"] = shadows.atlas_size;
    shadows_node["csm_tile_size"] = shadows.csm_tile_size;
    shadows_node["local_tile_size"] = shadows.local_tile_size;
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

bool
render_config::load_with_cache(const vfs::rid& base, const vfs::rid& cache)
{
    auto& vfs = glob::glob_state().getr_vfs();
    if (vfs.exists(cache))
    {
        ALOG_INFO("Found cached render config at '{}'", cache.str());
        serialization::container container;
        if (serialization::read_container(cache, container))
        {
            if (auto s = container["shadows"]; s && s.IsMap())
            {
                extract_field(s, "pcf", shadows.pcf);
                extract_field(s, "bias", shadows.bias);
                extract_field(s, "normal_bias", shadows.normal_bias);
                extract_field(s, "pcf_world_radius", shadows.pcf_world_radius);
                extract_field(s, "hardware_pcf", shadows.hardware_pcf);
                extract_field(s, "depth_16bit", shadows.depth_16bit);
                extract_field(s, "cascade_count", shadows.cascade_count);
                extract_field(s, "distance", shadows.distance);
                extract_field(s, "atlas_size", shadows.atlas_size);
                extract_field(s, "csm_tile_size", shadows.csm_tile_size);
                extract_field(s, "local_tile_size", shadows.local_tile_size);

                if (!s["csm_tile_size"] && s["map_size"])
                {
                    shadows.csm_tile_size = s["map_size"].as<uint32_t>();
                    shadows.local_tile_size = shadows.csm_tile_size / 2;
                }
            }

            if (auto c = container["clusters"]; c && c.IsMap())
            {
                extract_field(c, "tile_size", clusters.tile_size);
                extract_field(c, "depth_slices", clusters.depth_slices);
                extract_field(c, "max_lights_per_cluster", clusters.max_lights_per_cluster);
            }

            if (auto l = container["lighting"]; l && l.IsMap())
            {
                extract_field(l, "directional", lighting.directional_enabled);
                extract_field(l, "local", lighting.local_enabled);
                extract_field(l, "baked", lighting.baked_enabled);
            }

            if (auto d = container["debug"]; d && d.IsMap())
            {
                extract_field(d, "editor_mode", debug.editor_mode);
                extract_field(d, "show_grid", debug.show_grid);
                extract_field(d, "light_wireframe", debug.light_wireframe);
                extract_field(d, "light_icons", debug.light_icons);
                extract_field(d, "frustum_culling", debug.frustum_culling);
            }

            if (auto rs = container["render_scale"]; rs && rs.IsMap())
            {
                extract_field(rs, "enabled", render_scale.enabled);
                extract_field(rs, "divisor", render_scale.divisor);
            }

            if (auto ol = container["outline"]; ol && ol.IsMap())
            {
                extract_field(ol, "enabled", outline.enabled);
                extract_field(ol, "depth_threshold", outline.depth_threshold);
                extract_field(ol, "normal_threshold", outline.normal_threshold);
            }

            validate();
            return true;
        }
    }

    // Fall back to base config (read via VFS so APK-asset backends work)
    return load(base);
}

bool
render_config::save_to_cache(const vfs::rid& cache) const
{
    glob::glob_state().getr_vfs().create_directories(vfs::rid(cache.mount_point(), ""));

    YAML::Node root;

    YAML::Node shadows_node;
    shadows_node["enabled"] = shadows.enabled;
    shadows_node["pcf"] = pcf_mode_to_string.at(shadows.pcf);
    shadows_node["bias"] = shadows.bias;
    shadows_node["normal_bias"] = shadows.normal_bias;
    shadows_node["pcf_world_radius"] = shadows.pcf_world_radius;
    shadows_node["hardware_pcf"] = shadows.hardware_pcf;
    shadows_node["depth_16bit"] = shadows.depth_16bit;
    shadows_node["cascade_count"] = shadows.cascade_count;
    shadows_node["distance"] = shadows.distance;
    shadows_node["atlas_size"] = shadows.atlas_size;
    shadows_node["csm_tile_size"] = shadows.csm_tile_size;
    shadows_node["local_tile_size"] = shadows.local_tile_size;
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

    return serialization::write_container(cache, root);
}

}  // namespace render
}  // namespace kryga
