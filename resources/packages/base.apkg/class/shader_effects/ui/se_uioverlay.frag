#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "gpu_types/gpu_generic_constants.h"

// Sample the ImGui font atlas from the GLOBAL bindless set (set 2), not a
// per-material descriptor set. The font's bindless index + sampler index arrive
// via push constants (see PushConstants below / draw_ui).
layout(set = KGPU_textures_descriptor_sets, binding = 0) uniform sampler static_samplers[KGPU_SAMPLER_COUNT];
layout(set = KGPU_textures_descriptor_sets, binding = 1) uniform texture2D bindless_textures[];

layout (location = 0) in vec2 in_UV;
layout (location = 1) in vec4 in_color;

layout (location = 0) out vec4 out_color;

// Must match se_uioverlay.vert byte-for-byte (shared push-constant range).
layout (push_constant) uniform PushConstants {
    vec2 scale;
    vec2 translate;
    uint tex_index;
    uint sampler_index;
} pc;

void main()
{
    out_color = in_color * texture(
        sampler2D(bindless_textures[nonuniformEXT(pc.tex_index)],
                  static_samplers[pc.sampler_index]),
        in_UV);
}
