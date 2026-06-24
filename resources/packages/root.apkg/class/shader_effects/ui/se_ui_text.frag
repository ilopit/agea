#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "gpu_types/gpu_generic_constants.h"

// Sample the font atlas from the GLOBAL bindless set (set 2). The atlas stores
// glyph coverage in the alpha channel (RGB = white); we modulate the per-glyph
// color by that coverage. tex_index / sampler_index arrive via push constants
// (see draw_ui_text). Layout mirrors se_uioverlay.frag.
layout(set = KGPU_textures_descriptor_sets, binding = 0) uniform sampler static_samplers[KGPU_SAMPLER_COUNT];
layout(set = KGPU_textures_descriptor_sets, binding = 1) uniform texture2D bindless_textures[];

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;

layout(location = 0) out vec4 out_color;

// Must match se_ui_text.vert byte-for-byte (shared push-constant range).
layout(push_constant) uniform UiTextPushConstants {
    vec4 rect_ndc;
    vec4 uv_rect;
    vec4 color;
    uint tex_index;
    uint sampler_index;
} pc;

void main()
{
    float coverage = texture(
        sampler2D(bindless_textures[nonuniformEXT(pc.tex_index)],
                  static_samplers[pc.sampler_index]),
        in_uv).a;

    out_color = vec4(in_color.rgb, in_color.a * coverage);
}
