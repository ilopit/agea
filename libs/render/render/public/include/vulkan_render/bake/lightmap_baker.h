#pragma once

#include "vulkan_render/bake/bake_types.h"
#include "vulkan_render/bake/bvh_builder.h"
#include "vulkan_render/utils/vulkan_buffer.h"

#include <gpu_types/gpu_bvh_types.h>

#include <utils/id.h>

#include <vector>
#include <cstdint>

namespace kryga
{
namespace render
{

class lightmap_baker
{
public:
    // Add a static mesh to the bake scene
    void
    add_mesh(const gpu::vertex_data* vertices,
             uint32_t vertex_count,
             const uint32_t* indices,
             uint32_t index_count);

    // Clear all added meshes
    void
    clear();

    // Run the full bake pipeline:
    // 1. Build BVH from all added meshes
    // 2. Upload BVH + triangle data to GPU
    // 3. Rasterize G-buffer (world pos + normal per lightmap texel)
    // 4. Dispatch direct lighting compute
    // 5. Dispatch indirect lighting compute (N bounces)
    // 6. Dispatch AO compute
    // 7. Dispatch denoise
    // 8. Read back results
    bake::bake_result
    bake(const bake::bake_settings& settings);

    // Get the final lightmap data after bake (RGBA16F, size = width * height * 8 bytes)
    const std::vector<uint8_t>&
    get_lightmap_data() const
    {
        return m_lightmap_data;
    }

    // Get the AO data after bake (R16F, size = width * height * 2 bytes)
    const std::vector<uint8_t>&
    get_ao_data() const
    {
        return m_ao_data;
    }

private:
    // Accumulated mesh data (before BVH build)
    std::vector<gpu::vertex_data> m_vertices;
    std::vector<uint32_t> m_indices;

    // Bake results
    std::vector<uint8_t> m_lightmap_data;
    std::vector<uint8_t> m_ao_data;
};

}  // namespace render
}  // namespace kryga
