#pragma once

#include <utils/dynamic_object_builder.h>

#include <gpu_types/gpu_light_types.h>
#include <gpu_types/gpu_camera_types.h>
#include <gpu_types/gpu_object_types.h>
#include <gpu_types/gpu_vertex_types.h>
#include <gpu_types/gpu_push_constants.h>

#include <glm_unofficial/glm.h>

namespace agea
{
namespace render
{

using gpu_data_index_type = uint32_t;
using gpu_index_data = uint32_t;

constexpr gpu_data_index_type INVALID_GPU_MATERIAL_DATA_INDEX = 1024;
constexpr gpu_data_index_type INVALID_GPU_INDEX = uint32_t(-1);

// Import GPU types from gpu_types library into render namespace
using gpu_camera_data = gpu::camera_data;
using gpu_directional_light_data = gpu::directional_light_data;
using gpu_universal_light_data = gpu::universal_light_data;
using gpu_object_data = gpu::object_data;
using gpu_vertex_data = gpu::vertex_data;

std::shared_ptr<utils::dynobj_layout>
get_default_vertex_inout_layout();

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