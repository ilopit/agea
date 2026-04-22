#include "voronoi_fracture/voronoi_fracture.h"

#include <algorithm>
#include <limits>
#include <random>
#include <unordered_map>

namespace kryga
{
namespace voronoi_fracture
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
};

aabb
compute_aabb(const gpu::vertex_data* verts, uint32_t count)
{
    aabb box;
    for (uint32_t i = 0; i < count; ++i)
    {
        box.expand(verts[i].position);
    }
    if (count == 0)
    {
        box.mn = glm::vec3(0.0f);
        box.mx = glm::vec3(0.0f);
    }
    return box;
}

std::vector<glm::vec3>
sample_seed_points(const aabb& box, uint32_t count, uint32_t seed)
{
    std::vector<glm::vec3> out;
    out.reserve(count);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> ux(box.mn.x, box.mx.x);
    std::uniform_real_distribution<float> uy(box.mn.y, box.mx.y);
    std::uniform_real_distribution<float> uz(box.mn.z, box.mx.z);

    for (uint32_t i = 0; i < count; ++i)
    {
        out.emplace_back(ux(rng), uy(rng), uz(rng));
    }
    return out;
}

uint32_t
nearest_seed(const glm::vec3& p, const std::vector<glm::vec3>& seeds)
{
    uint32_t best = 0;
    float best_d2 = std::numeric_limits<float>::max();
    for (uint32_t i = 0; i < seeds.size(); ++i)
    {
        float d2 = glm::dot(p - seeds[i], p - seeds[i]);
        if (d2 < best_d2)
        {
            best_d2 = d2;
            best = i;
        }
    }
    return best;
}

}  // namespace

fracture_result
fracture_mesh(const gpu::vertex_data* vertices,
              uint32_t vertex_count,
              const gpu::uint* indices,
              uint32_t index_count,
              const fracture_params& params)
{
    fracture_result result;

    if (vertex_count == 0 || index_count < 3 || params.cell_count == 0)
    {
        return result;
    }

    auto box = compute_aabb(vertices, vertex_count);
    result.aabb_min = box.mn;
    result.aabb_max = box.mx;

    auto seeds = sample_seed_points(box, params.cell_count, params.seed);

    const uint32_t tri_count = index_count / 3;

    std::vector<std::vector<uint32_t>> cell_triangles(params.cell_count);

    for (uint32_t t = 0; t < tri_count; ++t)
    {
        uint32_t i0 = indices[3 * t + 0];
        uint32_t i1 = indices[3 * t + 1];
        uint32_t i2 = indices[3 * t + 2];

        glm::vec3 centroid = (vertices[i0].position + vertices[i1].position +
                              vertices[i2].position) /
                             3.0f;

        uint32_t cell = nearest_seed(centroid, seeds);
        cell_triangles[cell].push_back(t);
    }

    result.chunks.reserve(params.cell_count);

    for (uint32_t c = 0; c < params.cell_count; ++c)
    {
        const auto& tris = cell_triangles[c];
        if (tris.empty())
        {
            continue;
        }

        chunk ck;
        ck.seed_point = seeds[c];
        ck.vertices.reserve(tris.size() * 3);
        ck.indices.reserve(tris.size() * 3);

        std::unordered_map<uint32_t, uint32_t> remap;
        aabb cbox;

        for (uint32_t t : tris)
        {
            for (uint32_t k = 0; k < 3; ++k)
            {
                uint32_t src_idx = indices[3 * t + k];

                auto it = remap.find(src_idx);
                uint32_t local_idx;
                if (it == remap.end())
                {
                    local_idx = static_cast<uint32_t>(ck.vertices.size());
                    ck.vertices.push_back(vertices[src_idx]);
                    remap.emplace(src_idx, local_idx);
                    cbox.expand(vertices[src_idx].position);
                }
                else
                {
                    local_idx = it->second;
                }

                ck.indices.push_back(local_idx);
            }
        }

        ck.aabb_min = cbox.mn;
        ck.aabb_max = cbox.mx;

        result.chunks.push_back(std::move(ck));
    }

    return result;
}

}  // namespace voronoi_fracture
}  // namespace kryga
