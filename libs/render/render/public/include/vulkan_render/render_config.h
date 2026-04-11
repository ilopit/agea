#pragma once

#include <vulkan_render/render_enums.h>

#include <utils/path.h>
#include <vfs/rid.h>

#include <cstdint>

namespace kryga
{
namespace render
{

struct render_config
{
    struct shadow_cfg
    {
        pcf_mode pcf = pcf_mode::poisson16;
        float bias = 0.005f;
        float normal_bias = 0.03f;
        uint32_t cascade_count = 4;
        float distance = 200.0f;
        uint32_t map_size = 2048;
        bool enabled = true;
    } shadows;

    struct cluster_cfg
    {
        uint32_t tile_size = 128;
        uint32_t depth_slices = 12;
        uint32_t max_lights_per_cluster = 32;
    } clusters;

    struct lighting_cfg
    {
        bool directional_enabled = true;
        bool local_enabled = true;  // point + spot
        bool baked_enabled = true;  // lightmap GI
    } lighting;

    struct debug_cfg
    {
        bool show_grid = true;
        bool light_wireframe = true;
        bool light_icons = false;
        bool frustum_culling = true;
    } debug;

    // Clamp all fields to valid ranges
    void
    validate();

    bool
    load(const utils::path& path);

    bool
    save(const utils::path& path) const;

    // Load from cache VFS path if it exists, otherwise load base config
    bool
    load_with_cache(const vfs::rid& base, const vfs::rid& cache);

    // Save current state to cache VFS path
    bool
    save_to_cache(const vfs::rid& cache) const;
};

}  // namespace render
}  // namespace kryga
