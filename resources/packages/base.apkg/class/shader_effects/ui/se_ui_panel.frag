#version 450

// UI panel fragment shader — flat color with alpha blending.
// Push-const layout is identical to the vertex stage (Vulkan requires it).

layout(push_constant) uniform UiPushConstants {
    vec4 rect_ndc;
    vec4 color_opacity;
} pc;

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = in_color;
}
