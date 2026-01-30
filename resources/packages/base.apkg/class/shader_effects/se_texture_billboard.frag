#version 450
#include "common_frag.glsl"

void main()
{
    uint albedo_idx = constants.obj.texture_indices[KGPU_TEXTURE_SLOT_ALBEDO];
    uint sampler_idx = constants.obj.sampler_indices[KGPU_TEXTURE_SLOT_ALBEDO];
    out_color = sample_bindless_texture(albedo_idx, sampler_idx, in_tex_coord);
}