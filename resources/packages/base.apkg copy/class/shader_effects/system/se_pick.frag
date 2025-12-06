#version 450
#extension GL_GOOGLE_include_directive: enable
#include "common_frag.glsl"

void main()
{
    out_color = vec4(in_color, 1.0);
}