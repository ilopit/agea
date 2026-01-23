#version 450

layout(set = 2, binding = 0) uniform sampler2D textures[1];

// Input
layout (location = 0) in vec2 in_tex_coord;

layout (location = 0) out vec4 out_color;

void main() 
{
    out_color = texture(textures[0], in_tex_coord);
}