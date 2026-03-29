#version 450
#extension GL_GOOGLE_include_directive: enable
#include "common_frag.glsl"
#include "gpu_types/solid_color_alpha_material__gpu.h"

layout(std430, set = KGPU_materials_descriptor_sets, binding = 0) readonly buffer MaterialBuffer{
    solid_color_alpha_material__gpu objects[];
} dyn_material_buffer;

void main()
{
    solid_color_alpha_material__gpu material = dyn_material_buffer.objects[get_material_id()];
    out_color = vec4(material.diffuse, material.opacity);
}
