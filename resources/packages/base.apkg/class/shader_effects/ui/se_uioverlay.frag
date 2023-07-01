#version 450

layout(set = 0, binding = 0) uniform sampler2D fontSampler[];

layout (location = 0) in vec2 in_UV;
layout (location = 1) in vec4 in_color;

layout (location = 0) out vec4 out_color;

void main() 
{
    out_color = in_color * texture(fontSampler[0], in_UV);
}