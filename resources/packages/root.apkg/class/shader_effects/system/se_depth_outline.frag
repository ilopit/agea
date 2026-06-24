#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "gpu_types/gpu_generic_constants.h"

layout(set = KGPU_textures_descriptor_sets, binding = 0) uniform sampler static_samplers[KGPU_SAMPLER_COUNT];
layout(set = KGPU_textures_descriptor_sets, binding = 1) uniform texture2D bindless_textures[];

layout (location = 0) in vec2 in_tex_coord;
layout (location = 0) out vec4 out_color;

layout(push_constant, scalar) uniform Constants
{
    vec4 outline_color;
    vec2 texel_size;        // 1/width, 1/height of scene depth target
    float depth_threshold;  // first-order depth edge cutoff
    float normal_threshold; // second-order (crease) cutoff
    uint depth_texture_idx;
    float near_plane;
    float far_plane;
    uint _pad;
} pc;

float sample_raw_depth(vec2 uv)
{
    return texture(
        sampler2D(bindless_textures[nonuniformEXT(pc.depth_texture_idx)],
                  static_samplers[KGPU_SAMPLER_LINEAR_CLAMP]),
        uv).r;
}

// Linearize a hardware depth value to [near, far] in view space.
// Assumes standard reversed-Z-free perspective (0..1 depth range).
float linearize(float d)
{
    float n = pc.near_plane;
    float f = pc.far_plane;
    // Standard OpenGL-style linearization; works for 0..1 depth with reversed Z off.
    return (n * f) / (f - d * (f - n));
}

void main()
{
    vec2 uv = in_tex_coord;
    float d_c = sample_raw_depth(uv);
    if (d_c >= 1.0)
    {
        discard;
    }

    float d_l = sample_raw_depth(uv + vec2(-pc.texel_size.x, 0.0));
    float d_r = sample_raw_depth(uv + vec2( pc.texel_size.x, 0.0));
    float d_u = sample_raw_depth(uv + vec2(0.0, -pc.texel_size.y));
    float d_d = sample_raw_depth(uv + vec2(0.0,  pc.texel_size.y));

    float lz = linearize(d_c);
    float ll = linearize(d_l);
    float lr = linearize(d_r);
    float lu = linearize(d_u);
    float ld = linearize(d_d);

    // First-order: silhouettes where depth jumps between neighbors.
    // Scale threshold by center depth so far objects don't over-outline.
    float scale = max(lz, 0.1);
    float first_order =
        max(max(abs(ll - lz), abs(lr - lz)), max(abs(lu - lz), abs(ld - lz))) / scale;

    // Second-order: creases where depth is continuous but the slope changes
    // (e.g. two walls meeting). Works without sampling normals.
    float second_order =
        abs(ll + lr - 2.0 * lz) + abs(lu + ld - 2.0 * lz);
    second_order /= scale;

    if (first_order > pc.depth_threshold || second_order > pc.normal_threshold)
    {
        out_color = pc.outline_color;
    }
    else
    {
        discard;
    }
}
