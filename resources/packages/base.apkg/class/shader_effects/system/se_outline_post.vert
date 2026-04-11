#version 450

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;
layout (location = 4) in vec2 in_lightmap_uv;

layout (location = 0) out vec2 out_uv;

void main()
{
    out_uv = in_tex_coord;
    vec2 ndc = vec2(-1.0 + in_tex_coord.x * 2.0, -1.0 + in_tex_coord.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
