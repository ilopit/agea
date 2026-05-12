#pragma once

#include <gpu_types/gpu_probe_types.h>
#include <gpu_types/gpu_vertex_types.h>

#include <glm_unofficial/glm.h>

#include <vector>

namespace kryga
{
namespace render
{
namespace bake
{

struct probe_placement_params
{
    float spacing = 2.0f;  // distance between probes
    float margin = 0.5f;   // extra space around scene bounds
};

struct probe_placement_result
{
    std::vector<glm::vec3> positions;
    gpu::probe_grid_config grid_config;
};

// Place probes on a uniform grid covering the scene AABB.
// vertices/indices define the scene geometry (used to compute bounds).
probe_placement_result
place_probes_grid(const gpu::vertex_data* vertices,
                  uint32_t vertex_count,
                  const uint32_t* indices,
                  uint32_t index_count,
                  const probe_placement_params& params = {});

}  // namespace bake
}  // namespace render
}  // namespace kryga
