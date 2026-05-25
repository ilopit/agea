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
        float pcf_world_radius = 0.03f;
        bool hardware_pcf = true;
        bool depth_16bit = false;
        uint32_t cascade_count = 4;
        float distance = 200.0f;
        uint32_t atlas_size = KGPU_SHADOW_ATLAS_SIZE;
        uint32_t csm_tile_size = KGPU_SHADOW_CSM_TILE_SIZE;
        uint32_t local_tile_size = KGPU_SHADOW_LOCAL_TILE_SIZE;
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
        // Master gate for all editor-only visuals (grid, debug wireframes,
        // editor_only mesh components like light gizmo billboards). Runtime-
        // toggleable in the editor. Game builds never set this to true (no UI
        // to toggle it), so the rendering branch is naturally dead.
        bool editor_mode = true;
        bool show_grid = true;
        bool light_wireframe = true;
        bool light_icons = false;
        bool frustum_culling = true;
    } debug;

    // Render-scale: draw the scene into a reduced-resolution target, then
    // nearest-upscale it to the swapchain. UI overlays stay full-res.
    // Used for the "pixel art" look and for framerate scaling.
    struct render_scale_cfg
    {
        bool enabled = false;
        uint32_t divisor = 3;  // 1 = full-res; 3 = 1/3 per axis (1080p -> 360p)
    } render_scale;

    // Depth-based silhouette outline (Holland-style edge detection). Requires
    // render_scale.enabled=true in the current implementation — outline is drawn
    // in the composite pass.
    struct outline_cfg
    {
        bool enabled = false;
        float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        float depth_threshold = 0.08f;
        float normal_threshold = 0.35f;
    } outline;

    // Clamp all fields to valid ranges
    void
    validate();

    bool
    load(const vfs::rid& rid);

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
