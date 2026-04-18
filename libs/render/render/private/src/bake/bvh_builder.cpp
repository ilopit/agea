#include "vulkan_render/bake/bvh_builder.h"

#include <utils/kryga_log.h>

#include <algorithm>
#include <numeric>

namespace kryga
{
namespace render
{
namespace bake
{

namespace
{

struct aabb
{
    glm::vec3 mn{std::numeric_limits<float>::max()};
    glm::vec3 mx{std::numeric_limits<float>::lowest()};

    void
    expand(const glm::vec3& p)
    {
        mn = glm::min(mn, p);
        mx = glm::max(mx, p);
    }

    void
    expand(const aabb& other)
    {
        mn = glm::min(mn, other.mn);
        mx = glm::max(mx, other.mx);
    }

    float
    surface_area() const
    {
        glm::vec3 d = mx - mn;
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }

    glm::vec3
    centroid() const
    {
        return (mn + mx) * 0.5f;
    }
};

struct build_node
{
    aabb bounds;
    uint32_t left = 0;
    uint32_t right = 0;
    uint32_t first_tri = 0;
    uint32_t tri_count = 0;

    bool
    is_leaf() const
    {
        return tri_count > 0;
    }
};

constexpr uint32_t SAH_BINS = 12;
constexpr uint32_t MAX_LEAF_TRIS = 4;
constexpr float TRAVERSAL_COST = 1.0f;
constexpr float INTERSECT_COST = 1.5f;

uint32_t
build_recursive(std::vector<build_node>& nodes,
                std::vector<uint32_t>& tri_indices,
                const std::vector<aabb>& tri_bounds,
                const std::vector<glm::vec3>& tri_centroids,
                uint32_t begin,
                uint32_t end)
{
    uint32_t node_idx = static_cast<uint32_t>(nodes.size());
    nodes.emplace_back();

    uint32_t count = end - begin;

    // Compute bounds
    aabb bounds;
    for (uint32_t i = begin; i < end; ++i)
    {
        bounds.expand(tri_bounds[tri_indices[i]]);
    }
    nodes[node_idx].bounds = bounds;

    if (count <= MAX_LEAF_TRIS)
    {
        nodes[node_idx].first_tri = begin;
        nodes[node_idx].tri_count = count;
        return node_idx;
    }

    // SAH binned split
    aabb centroid_bounds;
    for (uint32_t i = begin; i < end; ++i)
    {
        centroid_bounds.expand(tri_centroids[tri_indices[i]]);
    }

    glm::vec3 extent = centroid_bounds.mx - centroid_bounds.mn;
    int best_axis = 0;
    if (extent.y > extent.x)
    {
        best_axis = 1;
    }
    if (extent.z > extent[best_axis])
    {
        best_axis = 2;
    }

    float axis_extent = extent[best_axis];

    // Degenerate case: all centroids at the same position
    if (axis_extent < 1e-6f)
    {
        nodes[node_idx].first_tri = begin;
        nodes[node_idx].tri_count = count;
        return node_idx;
    }

    struct bin
    {
        aabb bounds;
        uint32_t count = 0;
    };
    bin bins[SAH_BINS];

    float inv_extent = static_cast<float>(SAH_BINS) / axis_extent;

    for (uint32_t i = begin; i < end; ++i)
    {
        float c = tri_centroids[tri_indices[i]][best_axis];
        uint32_t b = std::min(
            static_cast<uint32_t>((c - centroid_bounds.mn[best_axis]) * inv_extent), SAH_BINS - 1);
        bins[b].count++;
        bins[b].bounds.expand(tri_bounds[tri_indices[i]]);
    }

    // Evaluate SAH cost for each split
    float best_cost = std::numeric_limits<float>::max();
    uint32_t best_split = 0;

    float parent_sa = bounds.surface_area();
    if (parent_sa <= 0.0f)
    {
        // Degenerate AABB (zero surface area) — force a leaf node
        nodes[node_idx].first_tri = begin;
        nodes[node_idx].tri_count = count;
        return node_idx;
    }

    for (uint32_t split = 1; split < SAH_BINS; ++split)
    {
        aabb left_bounds, right_bounds;
        uint32_t left_count = 0, right_count = 0;

        for (uint32_t j = 0; j < split; ++j)
        {
            if (bins[j].count > 0)
            {
                left_bounds.expand(bins[j].bounds);
                left_count += bins[j].count;
            }
        }
        for (uint32_t j = split; j < SAH_BINS; ++j)
        {
            if (bins[j].count > 0)
            {
                right_bounds.expand(bins[j].bounds);
                right_count += bins[j].count;
            }
        }

        if (left_count == 0 || right_count == 0)
        {
            continue;
        }

        float cost = TRAVERSAL_COST +
                     (left_bounds.surface_area() / parent_sa) * left_count * INTERSECT_COST +
                     (right_bounds.surface_area() / parent_sa) * right_count * INTERSECT_COST;

        if (cost < best_cost)
        {
            best_cost = cost;
            best_split = split;
        }
    }

    float leaf_cost = static_cast<float>(count) * INTERSECT_COST;
    if (best_cost >= leaf_cost || best_split == 0)
    {
        nodes[node_idx].first_tri = begin;
        nodes[node_idx].tri_count = count;
        return node_idx;
    }

    // Partition triangles by best split
    float split_pos = centroid_bounds.mn[best_axis] +
                      static_cast<float>(best_split) * axis_extent / static_cast<float>(SAH_BINS);

    auto mid_it =
        std::partition(tri_indices.begin() + begin,
                       tri_indices.begin() + end,
                       [&](uint32_t idx) { return tri_centroids[idx][best_axis] < split_pos; });

    uint32_t mid = static_cast<uint32_t>(mid_it - tri_indices.begin());

    if (mid == begin || mid == end)
    {
        mid = (begin + end) / 2;
    }

    uint32_t left = build_recursive(nodes, tri_indices, tri_bounds, tri_centroids, begin, mid);
    uint32_t right = build_recursive(nodes, tri_indices, tri_bounds, tri_centroids, mid, end);

    nodes[node_idx].left = left;
    nodes[node_idx].right = right;

    return node_idx;
}

}  // namespace

bvh_build_result
build_bvh(const gpu::vertex_data* vertices,
          uint32_t vertex_count,
          const uint32_t* indices,
          uint32_t index_count)
{
    bvh_build_result result;

    uint32_t tri_count = index_count / 3;
    if (tri_count == 0)
    {
        return result;
    }

    // Build triangle data
    result.triangles.resize(tri_count);
    std::vector<aabb> tri_bounds(tri_count);
    std::vector<glm::vec3> tri_centroids(tri_count);

    for (uint32_t t = 0; t < tri_count; ++t)
    {
        uint32_t i0 = indices[t * 3 + 0];
        uint32_t i1 = indices[t * 3 + 1];
        uint32_t i2 = indices[t * 3 + 2];

        auto& tri = result.triangles[t];
        tri.v0 = vertices[i0].position;
        tri.v1 = vertices[i1].position;
        tri.v2 = vertices[i2].position;
        tri.n0 = vertices[i0].normal;
        tri.n1 = vertices[i1].normal;
        tri.n2 = vertices[i2].normal;
        tri.lm_uv0 = vertices[i0].uv2;
        tri.lm_uv1 = vertices[i1].uv2;
        tri.lm_uv2 = vertices[i2].uv2;

        tri_bounds[t].expand(tri.v0);
        tri_bounds[t].expand(tri.v1);
        tri_bounds[t].expand(tri.v2);
        tri_centroids[t] = tri_bounds[t].centroid();
    }

    // Build BVH
    std::vector<uint32_t> tri_indices(tri_count);
    std::iota(tri_indices.begin(), tri_indices.end(), 0u);

    std::vector<build_node> build_nodes;
    build_nodes.reserve(tri_count * 2);

    build_recursive(build_nodes, tri_indices, tri_bounds, tri_centroids, 0, tri_count);

    // Reorder triangles by BVH leaf order
    std::vector<gpu::bake_triangle> ordered_tris(tri_count);
    for (uint32_t i = 0; i < tri_count; ++i)
    {
        ordered_tris[i] = result.triangles[tri_indices[i]];
    }
    result.triangles = std::move(ordered_tris);

    // Flatten build nodes to GPU format
    result.nodes.resize(build_nodes.size());
    for (size_t i = 0; i < build_nodes.size(); ++i)
    {
        auto& src = build_nodes[i];
        auto& dst = result.nodes[i];
        dst.aabb_min = src.bounds.mn;
        dst.aabb_max = src.bounds.mx;

        if (src.is_leaf())
        {
            dst.left_or_tri_idx = src.first_tri;
            dst.right_or_count = src.tri_count | KGPU_BVH_LEAF_FLAG;
        }
        else
        {
            dst.left_or_tri_idx = src.left;
            dst.right_or_count = src.right;
        }
    }

    ALOG_INFO("BVH built: {} nodes, {} triangles", result.nodes.size(), tri_count);

    return result;
}

}  // namespace bake
}  // namespace render
}  // namespace kryga
