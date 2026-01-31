#version 450
#extension GL_GOOGLE_include_directive: enable

#include "gpu_types/gpu_generic_constants.h"
#include "gpu_types/gpu_push_constants.h"

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;

layout (location = 0) out vec2 out_tex_coord;

// Push constants must be declared in both vertex and fragment for consistent pipeline layout
layout(push_constant) uniform Constants
{
    push_constants obj;
} constants;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    float x = in_tex_coord.x; 
    float y = in_tex_coord.y; 

    gl_Position = vec4(-1.0f + x*2.0f, -1.0f+y*2.0f, 0.0f, 1.0f);
    out_tex_coord = vec2(x, y);
}