#version 450
#extension GL_GOOGLE_include_directive: enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference_uvec2 : require

#include "gpu_types/gpu_push_constants_grid.h"
layout(push_constant) uniform Constants { push_constants_grid obj; } constants;
#include "bda_macros_grid.glsl"

#include "gpu_types/gpu_generic_constants.h"

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;
layout (location = 4) in vec2 in_lightmap_uv;

layout (location = 0) out vec3 out_near_point;
layout (location = 1) out vec3 out_far_point;

out gl_PerVertex
{
    vec4 gl_Position;
};

vec3 unproject_point(float x, float y, float z, mat4 inv_vp)
{
    vec4 clip = vec4(x, y, z, 1.0);
    vec4 world = inv_vp * clip;
    return world.xyz / world.w;
}

void main()
{
    float x = in_tex_coord.x;
    float y = in_tex_coord.y;

    vec2 ndc = vec2(-1.0 + x * 2.0, -1.0 + y * 2.0);

    mat4 inv_vp = inverse(dyn_camera_data.obj.projection * dyn_camera_data.obj.view);

    out_near_point = unproject_point(ndc.x, ndc.y, 0.0, inv_vp);
    out_far_point  = unproject_point(ndc.x, ndc.y, 1.0, inv_vp);

    gl_Position = vec4(ndc, 0.0, 1.0);
}
