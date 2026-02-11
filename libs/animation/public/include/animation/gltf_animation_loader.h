#pragma once

#include <utils/buffer.h>
#include <utils/id.h>

#include <gpu_types/gpu_vertex_types.h>
#include <glm_unofficial/glm.h>

#include <cstdint>
#include <string>
#include <vector>

namespace kryga
{
namespace animation
{

struct gltf_mesh_result
{
    std::vector<gpu::skinned_vertex_data> vertices;
    std::vector<uint32_t> indices;
};

struct gltf_load_result
{
    std::vector<gltf_mesh_result> meshes;
    std::vector<glm::mat4> inverse_bind_matrices;
    std::vector<std::string> joint_names;
};

class gltf_animation_loader
{
public:
    static bool
    load(const utils::buffer& buf, gltf_load_result& out);
};

}  // namespace animation
}  // namespace kryga
