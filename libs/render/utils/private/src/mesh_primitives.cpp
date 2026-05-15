#include <render/utils/mesh_primitives.h>

#include <cmath>

namespace kryga
{
namespace render
{

namespace
{
constexpr float PI = 3.14159265358979323846f;
}

primitive_mesh
generate_sphere(float radius, uint32_t stacks, uint32_t slices, float r, float g, float b)
{
    primitive_mesh mesh;
    mesh.vertices.reserve((stacks + 1) * (slices + 1));
    mesh.indices.reserve(stacks * slices * 6);

    for (uint32_t y = 0; y <= stacks; ++y)
    {
        float v = float(y) / float(stacks);
        float phi = v * PI;
        for (uint32_t x = 0; x <= slices; ++x)
        {
            float u = float(x) / float(slices);
            float theta = u * 2.0f * PI;

            float sp = std::sin(phi);
            float cp = std::cos(phi);
            float st = std::sin(theta);
            float ct = std::cos(theta);

            gpu::vertex_data vert{};
            vert.position[0] = radius * sp * ct;
            vert.position[1] = radius * cp;
            vert.position[2] = radius * sp * st;
            vert.normal[0] = sp * ct;
            vert.normal[1] = cp;
            vert.normal[2] = sp * st;
            vert.color[0] = r;
            vert.color[1] = g;
            vert.color[2] = b;
            vert.uv[0] = u;
            vert.uv[1] = v;
            vert.uv2[0] = 0.0f;
            vert.uv2[1] = 0.0f;
            mesh.vertices.push_back(vert);
        }
    }

    for (uint32_t y = 0; y < stacks; ++y)
    {
        for (uint32_t x = 0; x < slices; ++x)
        {
            uint32_t a = y * (slices + 1) + x;
            uint32_t b_idx = a + slices + 1;
            mesh.indices.push_back(a);
            mesh.indices.push_back(a + 1);
            mesh.indices.push_back(b_idx);
            mesh.indices.push_back(a + 1);
            mesh.indices.push_back(b_idx + 1);
            mesh.indices.push_back(b_idx);
        }
    }

    return mesh;
}

}  // namespace render
}  // namespace kryga
