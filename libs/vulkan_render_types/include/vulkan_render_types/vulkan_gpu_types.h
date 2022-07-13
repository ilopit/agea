#pragma once

#include <glm_unofficial/glm.h>

namespace agea
{
namespace render
{

struct mesh_push_constants
{
    glm::vec4 data;
    glm::mat4 render_matrix;
};

struct gpu_camera_data
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec3 pos;
};

struct gpu_scene_data
{
    glm::vec4 lights[4];
    glm::vec4 fog_color;      // w is for exponent
    glm::vec4 fog_distances;  // x for min, y for max, zw unused.
    glm::vec4 ambient_color;
    glm::vec4 sunlight_direction;  // w for sun power
    glm::vec4 sunlight_color;
};

struct gpu_object_data
{
    glm::mat4 model_matrix;
    glm::vec3 obj_pos;
    float dummy;
};

struct gpu_material_data
{
    float roughness = 0.5f;
    float metallic = 0.5f;
    float gamma = 0.8f;
    float albedo = 0.2f;
};

struct vertex_data
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;
};

}  // namespace render
}  // namespace agea