#version 450
#extension GL_GOOGLE_include_directive: enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "gpu_types/gpu_push_constants_pick.h"
layout(push_constant) uniform Constants { push_constants_pick obj; } constants;
#include "bda_macros_pick.glsl"

// Must match common_vert.glsl outputs for pipeline layout compatibility
layout (location = 0) in vec3 in_world_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;
layout (location = 4) in flat uint in_object_idx;

layout (location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(in_color, 1.0);
}
