#pragma once

#include <gpu_types/gpu_vertex_types.h>

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <vector>

namespace kryga
{
namespace voronoi_fracture
{

enum class fill_mode : uint8_t
{
    surface,  // open shells of the source surface (default)
    convex    // convex hull per chunk — closed, solid geometry
};

struct fracture_params
{
    uint32_t seed = 0;
    uint32_t cell_count = 8;
    fill_mode fill = fill_mode::surface;
    float roughness = 0.0f;
    uint32_t depth = 1;
    uint32_t detail = 1;
};

struct chunk
{
    std::vector<gpu::vertex_data> vertices;
    std::vector<gpu::uint> indices;
    glm::vec3 aabb_min{0.0f};
    glm::vec3 aabb_max{0.0f};
    glm::vec3 seed_point{0.0f};
};

struct fracture_result
{
    std::vector<chunk> chunks;
    glm::vec3 aabb_min{0.0f};
    glm::vec3 aabb_max{0.0f};
};

// Partitions a triangle mesh into Voronoi chunks.
//
// Seed points are sampled uniformly inside the source AABB with a deterministic
// RNG (params.seed). Each source triangle is assigned to the cell whose seed
// point is nearest to the triangle centroid — triangles straddling a cell
// boundary are not split.
//
// fill_mode::surface (default) — chunks are open shells of the source surface.
// fill_mode::convex — each chunk is replaced by the convex hull of its vertex
// positions, giving closed solid geometry with flat-shaded faces. Vertex color
// and UVs are reset (caller typically overrides color per chunk). Falls back to
// surface mode for degenerate chunks (< 4 non-coplanar vertices).
//
// Empty cells (no triangles assigned) are dropped from the result.
fracture_result
fracture_mesh(const gpu::vertex_data* vertices,
              uint32_t vertex_count,
              const gpu::uint* indices,
              uint32_t index_count,
              const fracture_params& params);

}  // namespace voronoi_fracture
}  // namespace kryga
