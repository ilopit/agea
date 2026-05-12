#pragma once

#include <vulkan_render/types/vulkan_gpu_types.h>

#include <utils/buffer.h>

#include <vector>

namespace kryga
{
namespace asset_importer
{
namespace uv2_generator
{

struct uv2_generate_params
{
    uint32_t max_chart_size = 0;   // 0 = auto
    float texels_per_unit = 0.0f;  // 0 = auto
    uint32_t padding = 4;          // padding between charts in texels (>=2 for bilinear)
    uint32_t resolution = 1024;    // target atlas resolution for packing
};

struct uv2_result
{
    std::vector<gpu::vertex_data> vertices;
    std::vector<gpu::uint> indices;
    uint32_t atlas_width = 0;
    uint32_t atlas_height = 0;
    bool success = false;
};

// Generate lightmap UV2 coordinates for a mesh using xatlas.
// Input vertices may be re-indexed (split at UV seams), so the output
// vertex/index buffers may differ in size from the input.
uv2_result
generate_uv2(const gpu::vertex_data* vertices,
             uint32_t vertex_count,
             const gpu::uint* indices,
             uint32_t index_count,
             const uv2_generate_params& params = {});

}  // namespace uv2_generator
}  // namespace asset_importer
}  // namespace kryga
