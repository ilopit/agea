#version 450

// UI text vertex shader — emits one screen-space glyph quad.
//
// No vertex buffer: gl_VertexIndex (6 verts, 2 triangles) builds a quad whose
// corners come from rect_ndc, with atlas UVs interpolated from uv_rect. One draw
// call per glyph (see draw_ui_text); the render side lays out the rects.

layout(push_constant) uniform UiTextPushConstants {
    vec4 rect_ndc;       // xy = min (top-left), zw = max (bottom-right), NDC
    vec4 uv_rect;        // xy = uv0, zw = uv1 (atlas, 0..1)
    vec4 color;          // rgba
    uint tex_index;      // bindless atlas index
    uint sampler_index;  // static sampler index
} pc;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    const vec2 corners[6] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 1.0)
    );

    vec2 c = corners[gl_VertexIndex];

    out_uv = mix(pc.uv_rect.xy, pc.uv_rect.zw, c);
    out_color = pc.color;
    gl_Position = vec4(mix(pc.rect_ndc.xy, pc.rect_ndc.zw, c), 0.0, 1.0);
}
