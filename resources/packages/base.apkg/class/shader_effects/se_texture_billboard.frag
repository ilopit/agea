#version 450
#extension GL_GOOGLE_include_directive: enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "gpu_types/gpu_push_constants_main.h"
layout(push_constant) uniform Constants { push_constants_main obj; } constants;
#include "bda_macros_main.glsl"
#include "common_frag.glsl"

#include "gpu_types/simple_texture_material__gpu.h"
layout(buffer_reference, scalar) readonly buffer BdaMaterialBuffer {
    simple_texture_material__gpu objects[];
};
#define dyn_material_buffer BdaMaterialBuffer(constants.obj.bdaf_material)

void main()
{
    uint _mi = get_material_id();
    vec4 color = sample_bindless_texture(dyn_material_buffer.objects[_mi].texture_indices[0], dyn_material_buffer.objects[_mi].sampler_indices[0], in_tex_coord);
    if (color.a < 0.1)
        discard;
    out_color = color;
}
