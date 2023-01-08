#pragma once

#include <stdint.h>

namespace agea
{
namespace utils
{

enum class agea_type
{
    t_nan,

    t_str,
    t_id,

    t_bool,

    t_i8,
    t_i16,
    t_i32,
    t_i64,

    t_u8,
    t_u16,
    t_u32,
    t_u64,

    t_f,
    t_d,

    t_vec2,
    t_vec3,
    t_vec4,

    t_txt,
    t_mat,
    t_msh,
    t_se,
    t_obj,
    t_com,

    t_color,
    t_buf,

    t_mat2,
    t_mat3,
    t_mat4,

    t_last = t_mat4 + 1,

};

uint32_t
get_agea_type_size(agea_type t);

}  // namespace utils
}  // namespace agea