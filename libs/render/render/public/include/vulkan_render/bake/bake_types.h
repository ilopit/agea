#pragma once

#include <gpu_types/gpu_light_types.h>
#include <vfs/rid.h>
#include <utils/path.h>

#include <cstdint>
#include <vector>

namespace kryga
{
namespace render
{
namespace bake
{

enum class bake_preset : uint32_t
{
    low,
    medium,
    high,
    maximum
};

// Persistent baker configuration — saved between sessions
struct bake_config
{
    uint32_t resolution = 1024;
    uint32_t samples_per_texel = 64;
    uint32_t bounce_count = 2;
    uint32_t denoise_iterations = 2;
    float ao_radius = 2.0f;
    float ao_intensity = 1.0f;
    bool bake_direct = true;
    bool bake_indirect = true;
    bool bake_ao = true;
    bool save_png = true;
    float texels_per_unit = 4.0f;
    int min_tile = 16;
    int max_tile = 256;
    float shadow_bias = 0.05f;
    uint32_t shadow_samples = 16;     // jittered rays per light for soft shadows
    float shadow_spread = 0.015f;     // angular spread of jitter (radians)
    uint32_t dilate_iterations = 3;   // gutter dilation passes

    void
    apply_preset(bake_preset preset);

    bool
    load(const utils::path& path);

    bool
    save(const utils::path& path) const;

    bool
    load_with_tmp(const utils::path& base_path);

    bool
    save_tmp(const utils::path& base_path) const;

    static utils::path
    tmp_path(const utils::path& base_path);
};

// Per-bake runtime settings — extends config with scene data and output paths
struct bake_settings : bake_config
{
    // Scene lights for baking. Must contain at least one directional light.
    std::vector<gpu::directional_light_data> directional_lights;
    std::vector<gpu::universal_light_data> local_lights;  // point + spot

    // Output paths. Empty rid = don't save to disk.
    vfs::rid output_lightmap;
    vfs::rid output_ao;
    bool output_png = false;
};

struct bake_result
{
    bool success = false;
    uint32_t atlas_width = 0;
    uint32_t atlas_height = 0;
    float bake_time_ms = 0.0f;
    uint32_t total_triangles = 0;
    uint32_t total_nodes = 0;
};

}  // namespace bake
}  // namespace render
}  // namespace kryga
