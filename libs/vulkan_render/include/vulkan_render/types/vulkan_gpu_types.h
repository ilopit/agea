#pragma once

#include <glm_unofficial/glm.h>
#include <utils/dynamic_object_builder.h>

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
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 ambient;
    alignas(16) glm::vec3 diffuse;
    alignas(16) glm::vec3 specular;
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

struct gpu_type
{
    enum id
    {
        g_nan = 1024,

        g_bool,
        g_int,
        g_unsigned,
        g_float,
        g_double,

        g_vec2,
        g_vec3,
        g_vec4,

        g_mat2,
        g_mat3,
        g_mat4,

        g_color,

        g_last = g_color + 1
    };

    static uint32_t
    size(gpu_type::id t);

    template <typename T>
    static constexpr id
    decode(const T&)
    {
        // clang-format off
        if      constexpr (std::is_same<T, float>::value)     { return g_float; }
        else if constexpr (std::is_same<T, double>::value)    { return g_double; }
        else if constexpr (std::is_same<T, uint32_t>::value)  { return g_unsigned; }
        else if constexpr (std::is_same<T, int32_t>::value)   { return g_int; }
        else if constexpr (std::is_same<T, glm::vec2>::value) { return g_vec2; }
        else if constexpr (std::is_same<T, glm::vec3>::value) { return g_vec3; }

        // clang-format on

        return g_nan;
    }

    template <typename T>
    static constexpr uint32_t
    decode_as_int(const T& v)
    {
        return (uint32_t)decode(v);
    }
};

using gpu_dynobj_builder = utils::dynamic_object_layout_sequence_builder<gpu_type>;

}  // namespace render
}  // namespace agea