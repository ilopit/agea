#version 450
#extension GL_GOOGLE_include_directive: enable

#include "gpu_types/gpu_push_constants_main.h"
layout(push_constant) uniform Constants { push_constants_main obj; } constants;
#include "descriptor_bindings_common.glsl"
#include "common_frag.glsl"

#include "gpu_types/simple_texture_material__gpu.h"
layout(set = KGPU_materials_descriptor_sets, binding = 0, scalar) readonly buffer MaterialBuffer
{
    simple_texture_material__gpu objects[];
} dyn_material_buffer;

void main()
{
    uint _mi = get_material_id();
    vec4 color = sample_bindless_texture(dyn_material_buffer.objects[_mi].texture_indices[0], dyn_material_buffer.objects[_mi].sampler_indices[0], in_tex_coord);
    if (color.a < 0.1)
        discard;
    out_color = color;
}
