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
        bool hardware_pcf_local = false;
        bool depth_16bit = true;
        uint32_t cascade_count = 4;
        float distance = 200.0f;
        uint32_t atlas_size = KGPU_SHADOW_ATLAS_SIZE;
        uint32_t csm_tile_size = KGPU_SHADOW_CSM_TILE_SIZE;
        uint32_t local_tile_size = KGPU_SHADOW_LOCAL_TILE_SIZE;
        uint32_t max_local_lights = KGPU_MAX_SHADOWED_LOCAL_LIGHTS;
        bool enabled = true;

        uint32_t
        max_cascades() const;
        uint32_t
        max_csm_tile() const;
        uint32_t
        max_local_tile() const;
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

    // How many frames the CPU runs ahead of the GPU. The renderer recreates the
    // swapchain to hold this many images (keeping frames_in_flight == image
    // count), clamped to the surface's supported range. Lower = less GPU memory
    // (fewer swapchain images + fewer per-frame buffer sets).
    uint32_t frames_in_flight = 3;

    // Explicit vsync choice, independent of frames_in_flight. mailbox requires
    // >=3 images, so picking it with frames_in_flight < 3 forces the image count
    // (and thus frames_in_flight) up to 3. Platform default: mailbox on desktop
    // for low latency, fifo on mobile to cut power/thermal load.
#if defined(__ANDROID__)
    present_mode present = present_mode::fifo;
#else
    present_mode present = present_mode::mailbox;
#endif

    // Present-wait pacing: bound how many frames the CPU may run ahead of the
    // display. Before each frame the render thread blocks on vkWaitForPresentKHR
    // until all but this many presents have been displayed, collapsing the FIFO
    // render-ahead queue that is the dominant present latency (latency drops from
    // ~(frames_in_flight+1) frame_times toward ~(present_pace_frames+1)). 0 = off
    // (legacy unthrottled behavior); 1 = tightest latency, least CPU/GPU overlap;
    // 2 = one frame of overlap (safer for heavy scenes). No-op when the driver
    // lacks VK_KHR_present_wait, and applied ONLY under FIFO — mailbox/immediate
    // have no render-ahead queue to bound and are left untouched (see
    // render_device::wait_present_pacing).
    // Default 2: ~35% lower FIFO latency while keeping a full frame of CPU/GPU
    // overlap so a heavy frame can't drop the cadence to 30fps; set 1 for the
    // tightest (one-frame) latency on light scenes. Clamped to <= frames_in_flight
    // by validate() (frames_in_flight has priority): pacing deeper than the buffer
    // can hold is a no-op, so the buffer depth is the ceiling.
    uint32_t present_pace_frames = 2;

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
