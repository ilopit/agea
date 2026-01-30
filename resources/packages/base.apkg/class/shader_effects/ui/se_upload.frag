#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "gpu_types/gpu_generic_constants.h"
#include "gpu_types/gpu_push_constants.h"

layout(set = KGPU_textures_descriptor_sets, binding = 0) uniform sampler static_samplers[KGPU_SAMPLER_COUNT];
layout(set = KGPU_textures_descriptor_sets, binding = 1) uniform texture2D bindless_textures[];

layout (location = 0) in vec2 in_tex_coord;

layout (location = 0) out vec4 out_color;

layout(push_constant) uniform Constants
{
    push_constants obj;
} constants;

void main()
{
    uint tex_idx = constants.obj.texture_indices[KGPU_TEXTURE_SLOT_ALBEDO];
    uint sampler_idx = constants.obj.sampler_indices[KGPU_TEXTURE_SLOT_ALBEDO];
    if (tex_idx == 0xFFFFFFFFu)
        out_color = vec4(1.0, 0.0, 1.0, 1.0);
    else
        out_color = texture(
            sampler2D(bindless_textures[nonuniformEXT(tex_idx)],
                      static_samplers[sampler_idx]),
            in_tex_coord);
}