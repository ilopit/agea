#pragma once

#include <glm_unofficial/glm.h>

namespace agea
{
namespace render
{

using gpu_data_index_type = uint32_t;
using gpu_index_data = uint32_t;

constexpr gpu_data_index_type INVALID_GPU_MATERIAL_DATA_INDEX = 1024;
constexpr gpu_data_index_type INVALID_GPU_INDEX = uint32_t(-1);

struct gpu_camera_data
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec3 position;
};

struct gpu_scene_data
{
    glm::vec4 lights[4];
    glm::vec4 lights_color;
    glm::vec4 lights_position;
};

struct gpu_object_data
{
    glm::mat4 model_matrix;
    glm::mat4 normal_matrix;
    glm::vec3 obj_pos;
    float dummy;
};

struct gpu_vertex_data
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;
};

struct gpu_push_constants
{
    alignas(16) gpu_data_index_type mat_id;
};

}  // namespace render
}  // namespace agea