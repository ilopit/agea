#version 450
#include "common_frag.glsl"

layout(set = 2, binding = 0) uniform sampler2D tex1[1];

void main()
{
    vec3 object_color = texture(tex1[0], in_tex_coord).xyz;
    out_color = vec4(object_color, 1.0);
}