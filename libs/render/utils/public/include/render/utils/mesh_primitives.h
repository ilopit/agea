#pragma once

#include <gpu_types/gpu_vertex_types.h>

#include <cstdint>
#include <vector>

namespace kryga
{
namespace render
{

struct primitive_mesh
{
    std::vector<gpu::vertex_data> vertices;
    std::vector<gpu::uint> indices;
};

primitive_mesh
generate_sphere(float radius = 0.5f,
                uint32_t stacks = 24,
                uint32_t slices = 32,
                float r = 1.0f,
                float g = 1.0f,
                float b = 1.0f);

// Unit cone: apex at origin, opens along -Y, height=1, base radius=1.
// Scale and orient per spot light at draw time.
primitive_mesh
generate_cone(uint32_t slices = 24,
              float r = 1.0f,
              float g = 1.0f,
              float b = 1.0f);

}  // namespace render
}  // namespace kryga
