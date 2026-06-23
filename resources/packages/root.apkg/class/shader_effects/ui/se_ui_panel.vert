#version 450

// UI panel vertex shader — emits a screen-space quad.
//
// No vertex buffer: uses gl_VertexIndex (6 verts, 2 triangles) to construct a
// quad whose corners come from the push-constant rect_ndc.
//
// rect_ndc layout: xy = min (bottom-left) in NDC, zw = max (top-right) in NDC.

layout(push_constant) uniform UiPushConstants {
    vec4 rect_ndc;       // min.x, min.y, max.x, max.y
    vec4 color_opacity;  // r, g, b, a
} pc;

layout(location = 0) out vec4 out_color;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    // TRIANGLE_LIST: 6 indices for 2 triangles making the quad
    // (min,min) (max,min) (max,max) | (min,min) (max,max) (min,max)
    const vec2 corners[6] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 1.0)
    );

    vec2 c = corners[gl_VertexIndex];
    vec2 ndc = mix(pc.rect_ndc.xy, pc.rect_ndc.zw, c);

    out_color = pc.color_opacity;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
