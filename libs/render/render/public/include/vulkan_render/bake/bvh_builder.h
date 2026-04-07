#pragma once

#include <gpu_types/gpu_bvh_types.h>
#include <gpu_types/gpu_vertex_types.h>

#include <vector>

namespace kryga
{
namespace render
{
namespace bake
{

struct bvh_build_result
{
    std::vector<gpu::bvh_node> nodes;
    std::vector<gpu::bake_triangle> triangles;
};

// Build a BVH from static mesh data using Surface Area Heuristic.
// Vertices are expected to have lightmap UVs in uv2.
// Triangles defined by index buffer (3 indices per triangle).
bvh_build_result
build_bvh(const gpu::vertex_data* vertices,
          uint32_t vertex_count,
          const uint32_t* indices,
          uint32_t index_count);

}  // namespace bake
}  // namespace render
}  // namespace kryga
