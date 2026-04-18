#version 450
#extension GL_GOOGLE_include_directive: enable

#include "gpu_types/gpu_push_constants_main.h"
layout(push_constant) uniform Constants { push_constants_main obj; } constants;
#include "descriptor_bindings_common.glsl"
#include "common_frag.glsl"

#include "gpu_types/solid_color_alpha_material__gpu.h"
layout(set = KGPU_materials_descriptor_sets, binding = 0, scalar) readonly buffer MaterialBuffer
{
    solid_color_alpha_material__gpu objects[];
} dyn_material_buffer;

void main()
{
    uint _mi = get_material_id();
    out_color = vec4(dyn_material_buffer.objects[_mi].diffuse, dyn_material_buffer.objects[_mi].opacity);
}
