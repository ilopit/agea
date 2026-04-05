#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "gpu_types/gpu_push_constants_main.h"
layout(push_constant) uniform Constants { push_constants_main obj; } constants;
#include "bda_macros_main.glsl"
#include "common_vert.glsl"

void main()
{
    uint obj_idx = get_object_index(constants.obj.instance_base);
    mat4 model = dyn_object_buffer.objects[obj_idx].model;

    vec4 worldPos = model * vec4(in_position, 1.0);
    gl_Position = dyn_camera_data.obj.projection * dyn_camera_data.obj.view * worldPos;

    out_world_pos = worldPos.xyz;
    out_normal = in_normal;
    out_color = in_color;
    out_tex_coord = in_tex_coord;
    out_object_idx = obj_idx;
}
