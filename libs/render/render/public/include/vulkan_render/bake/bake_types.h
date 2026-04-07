#pragma once

#include <gpu_types/gpu_light_types.h>
#include <vfs/rid.h>

#include <cstdint>
#include <vector>

namespace kryga
{
namespace render
{
namespace bake
{

struct bake_settings
{
    uint32_t resolution = 1024;           // lightmap atlas resolution
    uint32_t samples_per_texel = 64;      // ray samples per texel
    uint32_t bounce_count = 2;            // indirect lighting bounces
    uint32_t denoise_iterations = 2;      // bilateral filter passes
    float ao_radius = 2.0f;              // ambient occlusion ray max distance
    float ao_intensity = 1.0f;           // AO strength multiplier
    bool bake_direct = true;
    bool bake_indirect = true;
    bool bake_ao = true;

    // Scene lights for baking. Must contain at least one light.
    std::vector<gpu::directional_light_data> directional_lights;

    // Output paths. Empty rid = don't save to disk.
    vfs::rid output_lightmap;  // e.g. vfs::rid("tmp://baked/lightmap.bin")
    vfs::rid output_ao;        // e.g. vfs::rid("tmp://baked/ao.bin")
    bool output_png = false;   // Also save .png previews (8-bit, for visual inspection)
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
