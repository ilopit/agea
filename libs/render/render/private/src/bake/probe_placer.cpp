#include "vulkan_render/bake/probe_placer.h"

#include <utils/kryga_log.h>

#include <algorithm>
#include <cmath>

namespace kryga
{
namespace render
{
namespace bake
{

probe_placement_result
place_probes_grid(const gpu::vertex_data* vertices,
                  uint32_t vertex_count,
                  const uint32_t* indices,
                  uint32_t index_count,
                  const probe_placement_params& params)
{
    probe_placement_result result;

    if (vertex_count == 0)
    {
        return result;
    }

    // Compute scene AABB
    glm::vec3 scene_min(std::numeric_limits<float>::max());
    glm::vec3 scene_max(std::numeric_limits<float>::lowest());

    for (uint32_t i = 0; i < vertex_count; ++i)
    {
        scene_min = glm::min(scene_min, vertices[i].position);
        scene_max = glm::max(scene_max, vertices[i].position);
    }

    // Expand by margin
    scene_min -= glm::vec3(params.margin);
    scene_max += glm::vec3(params.margin);

    float spacing = params.spacing;

    uint32_t nx =
        std::max(1u, static_cast<uint32_t>(std::ceil((scene_max.x - scene_min.x) / spacing))) + 1;
    uint32_t ny =
        std::max(1u, static_cast<uint32_t>(std::ceil((scene_max.y - scene_min.y) / spacing))) + 1;
    uint32_t nz =
        std::max(1u, static_cast<uint32_t>(std::ceil((scene_max.z - scene_min.z) / spacing))) + 1;

    result.positions.reserve(nx * ny * nz);

    for (uint32_t z = 0; z < nz; ++z)
    {
        for (uint32_t y = 0; y < ny; ++y)
        {
            for (uint32_t x = 0; x < nx; ++x)
            {
                glm::vec3 pos = scene_min + glm::vec3(x, y, z) * spacing;
                result.positions.push_back(pos);
            }
        }
    }

    // Fill grid config
    auto& gc = result.grid_config;
    gc.grid_min = scene_min;
    gc.grid_max = scene_max;
    gc.spacing = spacing;
    gc.probe_count = static_cast<uint32_t>(result.positions.size());
    gc.grid_size_x = nx;
    gc.grid_size_y = ny;
    gc.grid_size_z = nz;

    ALOG_INFO("probe_placer: placed {} probes ({}x{}x{}) with spacing {:.1f}",
              result.positions.size(),
              nx,
              ny,
              nz,
              spacing);

    return result;
}

}  // namespace bake
}  // namespace render
}  // namespace kryga
