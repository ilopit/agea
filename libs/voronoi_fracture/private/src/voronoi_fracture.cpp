#include "voronoi_fracture/voronoi_fracture.h"

#include <manifold/manifold.h>
#include <voro++.hh>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <unordered_map>

namespace kryga
{
namespace voronoi_fracture
{

namespace
{

// ── Shared helpers ──────────────────────────────────────────────────────────

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
    for (uint32_t i = 0; i < static_cast<uint32_t>(seeds.size()); ++i)
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

// ── Roughness helpers ───────────────────────────────────────────────────────

uint32_t
lattice_hash(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
    uint32_t h = seed;
    h ^= uint32_t(x) * 2654435761u;
    h ^= uint32_t(y) * 2246822519u;
    h ^= uint32_t(z) * 3266489917u;
    h = (h ^ (h >> 16)) * 0x45d9f3bu;
    h = h ^ (h >> 16);
    return h;
}

float
lattice_value(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
    return float(lattice_hash(x, y, z, seed) & 0xFFFFu) / 32768.0f - 1.0f;
}

float
smooth_step(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

float
value_noise(const glm::vec3& p, uint32_t seed)
{
    int32_t ix = int32_t(std::floor(p.x));
    int32_t iy = int32_t(std::floor(p.y));
    int32_t iz = int32_t(std::floor(p.z));
    float fx = smooth_step(p.x - float(ix));
    float fy = smooth_step(p.y - float(iy));
    float fz = smooth_step(p.z - float(iz));

    float c000 = lattice_value(ix, iy, iz, seed);
    float c100 = lattice_value(ix + 1, iy, iz, seed);
    float c010 = lattice_value(ix, iy + 1, iz, seed);
    float c110 = lattice_value(ix + 1, iy + 1, iz, seed);
    float c001 = lattice_value(ix, iy, iz + 1, seed);
    float c101 = lattice_value(ix + 1, iy, iz + 1, seed);
    float c011 = lattice_value(ix, iy + 1, iz + 1, seed);
    float c111 = lattice_value(ix + 1, iy + 1, iz + 1, seed);

    float x00 = glm::mix(c000, c100, fx);
    float x10 = glm::mix(c010, c110, fx);
    float x01 = glm::mix(c001, c101, fx);
    float x11 = glm::mix(c011, c111, fx);
    float y0 = glm::mix(x00, x10, fy);
    float y1 = glm::mix(x01, x11, fy);
    return glm::mix(y0, y1, fz);
}

float
fbm_noise(const glm::vec3& p, uint32_t seed)
{
    float v = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    for (int i = 0; i < 3; ++i)
    {
        v += amp * value_noise(p * freq, seed + uint32_t(i) * 7919u);
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return v / 1.75f;
}

struct sub_tri
{
    glm::vec3 a, b, c;
};

void
subdivide_triangle(const sub_tri& t, int levels, std::vector<sub_tri>& out)
{
    if (levels <= 0)
    {
        out.push_back(t);
        return;
    }
    glm::vec3 mab = (t.a + t.b) * 0.5f;
    glm::vec3 mbc = (t.b + t.c) * 0.5f;
    glm::vec3 mca = (t.c + t.a) * 0.5f;
    subdivide_triangle({t.a, mab, mca}, levels - 1, out);
    subdivide_triangle({mab, t.b, mbc}, levels - 1, out);
    subdivide_triangle({mca, mbc, t.c}, levels - 1, out);
    subdivide_triangle({mab, mbc, mca}, levels - 1, out);
}

// ── Manifold integration ────────────────────────────────────────────────────

manifold::Manifold
build_input_manifold(const gpu::vertex_data* verts,
                     uint32_t vert_count,
                     const gpu::uint* indices,
                     uint32_t idx_count)
{
    // Deduplicate positions: Manifold needs proper topology, not UV-seam splits.
    struct pos_key
    {
        int32_t x, y, z;
        bool
        operator==(const pos_key& o) const
        {
            return x == o.x && y == o.y && z == o.z;
        }
    };
    struct key_hash
    {
        size_t
        operator()(const pos_key& k) const
        {
            size_t h = std::hash<int32_t>{}(k.x);
            h ^= std::hash<int32_t>{}(k.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int32_t>{}(k.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    constexpr float inv_eps = 1e5f;
    std::unordered_map<pos_key, uint32_t, key_hash> dedup;
    std::vector<float> props;
    std::vector<uint32_t> remap(vert_count);

    for (uint32_t i = 0; i < vert_count; ++i)
    {
        auto& p = verts[i].position;
        pos_key k{int32_t(std::round(p.x * inv_eps)),
                  int32_t(std::round(p.y * inv_eps)),
                  int32_t(std::round(p.z * inv_eps))};
        auto [it, ok] = dedup.try_emplace(k, uint32_t(props.size() / 3));
        if (ok)
        {
            props.push_back(p.x);
            props.push_back(p.y);
            props.push_back(p.z);
        }
        remap[i] = it->second;
    }

    std::vector<uint32_t> tris;
    tris.reserve(idx_count);
    for (uint32_t i = 0; i + 2 < idx_count; i += 3)
    {
        uint32_t a = remap[indices[i]];
        uint32_t b = remap[indices[i + 1]];
        uint32_t c = remap[indices[i + 2]];
        if (a != b && b != c && a != c)
        {
            tris.push_back(a);
            tris.push_back(b);
            tris.push_back(c);
        }
    }

    manifold::MeshGL mesh;
    mesh.numProp = 3;
    mesh.vertProperties = std::move(props);
    mesh.triVerts = std::move(tris);

    return manifold::Manifold(mesh);
}

manifold::Manifold
build_cell_manifold(voro::voronoicell_neighbor& cell, double px, double py, double pz)
{
    std::vector<double> raw_verts;
    cell.vertices(px, py, pz, raw_verts);
    int nv = cell.p;

    std::vector<int> fv;
    cell.face_vertices(fv);

    manifold::MeshGL mesh;
    mesh.numProp = 3;
    mesh.vertProperties.resize(nv * 3);
    for (int i = 0; i < nv; ++i)
    {
        mesh.vertProperties[i * 3 + 0] = float(raw_verts[i * 3 + 0]);
        mesh.vertProperties[i * 3 + 1] = float(raw_verts[i * 3 + 1]);
        mesh.vertProperties[i * 3 + 2] = float(raw_verts[i * 3 + 2]);
    }

    // Fan-triangulate each convex face.
    // Voro++ lists face vertices CCW from outside → outward normals via right-hand rule.
    size_t pos = 0;
    while (pos < fv.size())
    {
        int count = fv[pos++];
        for (int j = 1; j + 1 < count; ++j)
        {
            mesh.triVerts.push_back(uint32_t(fv[pos]));
            mesh.triVerts.push_back(uint32_t(fv[pos + j]));
            mesh.triVerts.push_back(uint32_t(fv[pos + j + 1]));
        }
        pos += count;
    }

    return manifold::Manifold(mesh);
}

uint32_t
find_original_id(const manifold::MeshGL& mesh, uint32_t tri)
{
    if (mesh.runOriginalID.empty())
    {
        return 0;
    }
    auto it = std::upper_bound(mesh.runIndex.begin(), mesh.runIndex.end(), tri);
    size_t run = size_t(it - mesh.runIndex.begin()) - 1;
    return mesh.runOriginalID[run];
}

void
emit_flat_triangle(const glm::vec3 p[3], chunk& ck, aabb& box)
{
    glm::vec3 fn = glm::cross(p[1] - p[0], p[2] - p[0]);
    float fl = glm::length(fn);
    fn = fl > 1e-8f ? fn / fl : glm::vec3(0, 1, 0);

    uint32_t base = uint32_t(ck.vertices.size());
    for (int k = 0; k < 3; ++k)
    {
        gpu::vertex_data v{};
        v.position = p[k];
        v.normal = fn;
        v.color = glm::vec3(1.0f);
        ck.vertices.push_back(v);
        ck.indices.push_back(base + k);
        box.expand(p[k]);
    }
}

void
manifold_to_chunk(const manifold::MeshGL& mesh,
                  uint32_t input_original_id,
                  float roughness,
                  uint32_t roughness_seed,
                  const glm::vec3& chunk_center,
                  chunk& ck,
                  aabb& box)
{
    uint32_t num_tris = mesh.NumTri();

    for (uint32_t t = 0; t < num_tris; ++t)
    {
        glm::vec3 p[3];
        for (int k = 0; k < 3; ++k)
        {
            uint32_t vi = mesh.triVerts[t * 3 + k];
            p[k] = {mesh.vertProperties[vi * 3 + 0],
                    mesh.vertProperties[vi * 3 + 1],
                    mesh.vertProperties[vi * 3 + 2]};
        }

        emit_flat_triangle(p, ck, box);
    }
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

    if (params.fill == fill_mode::convex)
    {
        auto input_mf = build_input_manifold(vertices, vertex_count, indices, index_count);
        if (input_mf.Status() != manifold::Manifold::Error::NoError)
        {
            return result;
        }

        auto input_id = input_mf.OriginalID();

        uint32_t detail = std::max(1u, params.detail);
        uint32_t fine_count = params.cell_count * detail;

        float pad = 0.01f * glm::length(box.mx - box.mn);
        if (pad < 1e-6f)
        {
            pad = 0.01f;
        }

        auto fine_seeds = sample_seed_points(box, fine_count, params.seed);

        std::vector<glm::vec3> group_centers;
        std::vector<uint32_t> cell_group(fine_count);

        if (detail > 1)
        {
            group_centers = sample_seed_points(box, params.cell_count, params.seed + 99991);
            for (uint32_t i = 0; i < fine_count; ++i)
            {
                cell_group[i] = nearest_seed(fine_seeds[i], group_centers);
            }
        }
        else
        {
            group_centers = fine_seeds;
            for (uint32_t i = 0; i < fine_count; ++i)
            {
                cell_group[i] = i;
            }
        }

        voro::container con(double(box.mn.x - pad),
                            double(box.mx.x + pad),
                            double(box.mn.y - pad),
                            double(box.mx.y + pad),
                            double(box.mn.z - pad),
                            double(box.mx.z + pad),
                            6,
                            6,
                            6,
                            false,
                            false,
                            false,
                            8);

        for (uint32_t i = 0; i < fine_count; ++i)
        {
            con.put(
                int(i), double(fine_seeds[i].x), double(fine_seeds[i].y), double(fine_seeds[i].z));
        }

        std::unordered_map<int, manifold::Manifold> cell_mfs;

        voro::voronoicell_neighbor vcell;
        voro::c_loop_all loop(con);

        if (loop.start())
        {
            do
            {
                double cx, cy, cz;
                int id = loop.pid();
                loop.pos(cx, cy, cz);

                if (!con.compute_cell(vcell, loop))
                {
                    continue;
                }

                auto mf = build_cell_manifold(vcell, cx, cy, cz);
                if (mf.Status() == manifold::Manifold::Error::NoError)
                {
                    cell_mfs.emplace(id, std::move(mf));
                }
            } while (loop.inc());
        }

        float leaf_roughness = (params.depth <= 1) ? params.roughness : 0.0f;

        result.chunks.reserve(params.cell_count);

        for (uint32_t g = 0; g < params.cell_count; ++g)
        {
            std::vector<manifold::Manifold> members;
            for (uint32_t i = 0; i < fine_count; ++i)
            {
                if (cell_group[i] != g)
                {
                    continue;
                }
                auto it = cell_mfs.find(int(i));
                if (it != cell_mfs.end())
                {
                    members.push_back(it->second);
                }
            }

            if (members.empty())
            {
                continue;
            }

            manifold::Manifold group_mf;
            if (members.size() == 1)
            {
                group_mf = std::move(members[0]);
            }
            else
            {
                group_mf = manifold::Manifold::BatchBoolean(members, manifold::OpType::Add);
            }

            if (group_mf.IsEmpty())
            {
                continue;
            }

            auto clipped = input_mf ^ group_mf;
            if (clipped.IsEmpty())
            {
                continue;
            }

            auto mesh = clipped.GetMeshGL();

            chunk ck;
            ck.seed_point = group_centers[g];
            aabb cbox;

            manifold_to_chunk(
                mesh, input_id, leaf_roughness, params.seed + g, group_centers[g], ck, cbox);

            if (!ck.vertices.empty())
            {
                for (auto& v : ck.vertices)
                {
                    v.position -= ck.seed_point;
                }
                ck.aabb_min = cbox.mn - ck.seed_point;
                ck.aabb_max = cbox.mx - ck.seed_point;
                result.chunks.push_back(std::move(ck));
            }
        }

        if (params.depth > 1)
        {
            std::vector<chunk> refined;
            uint32_t sub_cells = std::max(3u, params.cell_count / 2);

            for (uint32_t i = 0; i < uint32_t(result.chunks.size()); ++i)
            {
                auto& parent = result.chunks[i];

                fracture_params sub;
                sub.seed = params.seed * 31 + i * 97 + params.depth * 7919;
                sub.cell_count = sub_cells;
                sub.fill = fill_mode::convex;
                sub.roughness = params.roughness;
                sub.depth = params.depth - 1;
                sub.detail = 1;

                auto sub_result = fracture_mesh(parent.vertices.data(),
                                                uint32_t(parent.vertices.size()),
                                                parent.indices.data(),
                                                uint32_t(parent.indices.size()),
                                                sub);

                if (sub_result.chunks.empty())
                {
                    refined.push_back(std::move(parent));
                }
                else
                {
                    for (auto& sc : sub_result.chunks)
                    {
                        refined.push_back(std::move(sc));
                    }
                }
            }

            result.chunks = std::move(refined);
        }
    }
    else
    {
        std::vector<std::vector<uint32_t>> cell_triangles(params.cell_count);

        for (uint32_t t = 0; t < tri_count; ++t)
        {
            uint32_t i0 = indices[3 * t + 0];
            uint32_t i1 = indices[3 * t + 1];
            uint32_t i2 = indices[3 * t + 2];

            glm::vec3 centroid =
                (vertices[i0].position + vertices[i1].position + vertices[i2].position) / 3.0f;

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
    }

    return result;
}

}  // namespace voronoi_fracture
}  // namespace kryga
