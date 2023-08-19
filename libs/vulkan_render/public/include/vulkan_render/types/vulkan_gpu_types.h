#pragma once

#include <utils/dynamic_object_builder.h>

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

struct gpu_directional_light_data
{
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 ambient;
    alignas(16) glm::vec3 diffuse;
    alignas(16) glm::vec3 specular;
};

struct gpu_point_light_data
{
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 ambient;
    alignas(16) glm::vec3 diffuse;
    alignas(16) glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct gpu_spot_light_data
{
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 ambient;
    alignas(16) glm::vec3 diffuse;
    alignas(16) glm::vec3 specular;

    float cut_off;
    float outer_cut_off;
    float constant;
    float linear;
    float quadratic;
};

struct gpu_scene_data
{
    gpu_directional_light_data directional;
    gpu_point_light_data points[10];
    gpu_spot_light_data spots[10];
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

std::shared_ptr<utils::dynobj_layout>
get_default_vertex_inout_layout();

struct gpu_push_constants
{
    gpu_data_index_type material_id;
    gpu_data_index_type directional_light_id;

    gpu_data_index_type point_lights_size;
    gpu_data_index_type point_light_ids[10];

    gpu_data_index_type spot_lights_size;
    gpu_data_index_type spot_light_ids[10];
};

struct gpu_type
{
    enum id
    {
        nan = 0,

        g_bool = 1024,
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

    static const char*
    name(gpu_type::id t);

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

        return nan;
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