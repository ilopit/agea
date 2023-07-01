#version 450

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec2 in_UV;
layout (location = 2) in vec4 in_color;

layout (location = 0) out vec2 out_UV;
layout (location = 1) out vec4 out_color;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

layout (push_constant) uniform PushConstants {
    vec2 scale;
    vec2 translate;
} pushConstants;

void main() 
{
    out_UV = in_UV;
    out_color = in_color;
    gl_Position = vec4(in_pos * pushConstants.scale + pushConstants.translate, 0.0, 1.0);
}