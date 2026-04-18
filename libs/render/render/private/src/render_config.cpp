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
};

const std::unordered_map<pcf_mode, std::string> pcf_mode_to_string = {
    {pcf_mode::pcf_3x3, "3x3"},
    {pcf_mode::pcf_5x5, "5x5"},
    {pcf_mode::pcf_7x7, "7x7"},
    {pcf_mode::poisson16, "poisson16"},
    {pcf_mode::poisson32, "poisson32"},
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
    clamp_warn(pcf_val, (uint32_t)KGPU_PCF_MIN, (uint32_t)KGPU_PCF_MAX, "shadows.pcf");
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
    clamp_warn(shadows.map_size,
               (uint32_t)KGPU_SHADOW_MAP_SIZE_MIN,
               (uint32_t)KGPU_SHADOW_MAP_SIZE_MAX,
               "shadows.map_size");

    // Round to nearest power of two
    if (shadows.map_size & (shadows.map_size - 1))
    {
        uint32_t original = shadows.map_size;
        uint32_t v = shadows.map_size;
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;
        // Pick the closer power of two
        uint32_t lower = v >> 1;
        shadows.map_size = (v - original <= original - lower) ? v : lower;
        shadows.map_size = std::clamp(shadows.map_size,
                                      (uint32_t)KGPU_SHADOW_MAP_SIZE_MIN,
                                      (uint32_t)KGPU_SHADOW_MAP_SIZE_MAX);
        ALOG_WARN("render_config: 'shadows.map_size' value {} is not a power of two, rounded to {}",
                  original,
                  shadows.map_size);
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
}

// ============================================================================
// Load
// ============================================================================

bool
render_config::load(const utils::path& path)
{
    serialization::container container;
    if (!serialization::read_container(path, container))
    {
        ALOG_WARN("Failed to load render config from '{}', using defaults", path.str());
        return false;
    }

    if (auto shadows_node = container["shadows"]; shadows_node && shadows_node.IsMap())
    {
        extract_field(shadows_node, "enabled", shadows.enabled);
        extract_field(shadows_node, "pcf", shadows.pcf);
        extract_field(shadows_node, "bias", shadows.bias);
        extract_field(shadows_node, "normal_bias", shadows.normal_bias);
        extract_field(shadows_node, "cascade_count", shadows.cascade_count);
        extract_field(shadows_node, "distance", shadows.distance);
        extract_field(shadows_node, "map_size", shadows.map_size);
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
        extract_field(debug_node, "show_grid", debug.show_grid);
        extract_field(debug_node, "light_wireframe", debug.light_wireframe);
        extract_field(debug_node, "light_icons", debug.light_icons);
        extract_field(debug_node, "frustum_culling", debug.frustum_culling);
    }

    validate();

    ALOG_INFO("Loaded render config from '{}'", path.str());
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
    shadows_node["cascade_count"] = shadows.cascade_count;
    shadows_node["distance"] = shadows.distance;
    shadows_node["map_size"] = shadows.map_size;
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
    debug_node["show_grid"] = debug.show_grid;
    debug_node["light_wireframe"] = debug.light_wireframe;
    debug_node["light_icons"] = debug.light_icons;
    debug_node["frustum_culling"] = debug.frustum_culling;
    root["debug"] = debug_node;

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
                extract_field(s, "cascade_count", shadows.cascade_count);
                extract_field(s, "distance", shadows.distance);
                extract_field(s, "map_size", shadows.map_size);
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
                extract_field(d, "show_grid", debug.show_grid);
                extract_field(d, "light_wireframe", debug.light_wireframe);
                extract_field(d, "light_icons", debug.light_icons);
                extract_field(d, "frustum_culling", debug.frustum_culling);
            }

            validate();
            return true;
        }
    }

    // Fall back to base config
    auto rp = vfs.real_path(base);
    if (rp.has_value())
    {
        return load(APATH(rp.value()));
    }
    return false;
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
    shadows_node["cascade_count"] = shadows.cascade_count;
    shadows_node["distance"] = shadows.distance;
    shadows_node["map_size"] = shadows.map_size;
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
    debug_node["show_grid"] = debug.show_grid;
    debug_node["light_wireframe"] = debug.light_wireframe;
    debug_node["light_icons"] = debug.light_icons;
    debug_node["frustum_culling"] = debug.frustum_culling;
    root["debug"] = debug_node;

    return serialization::write_container(cache, root);
}

}  // namespace render
}  // namespace kryga
