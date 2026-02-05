#version 450

#include "common_frag.glsl"
#include "gpu_types/solid_color_material__gpu.h"

layout(std430, set = KGPU_materials_descriptor_sets, binding = 0) readonly buffer MaterialBuffer{
    solid_color_material__gpu objects[];
} dyn_material_buffer;

void main()
{
    // Use per-object material_id from object buffer
    solid_color_material__gpu material = dyn_material_buffer.objects[get_material_id()];

    out_color = vec4(1.0, 1.0, 1.0, 1.0);
}