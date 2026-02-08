#version 450
#extension GL_GOOGLE_include_directive: enable
#include "common_frag.glsl"
#include "gpu_types/pbr_material__gpu.h"

layout(std430, set = KGPU_materials_descriptor_sets, binding = 0) readonly buffer MaterialBuffer{
    pbr_material__gpu objects[];
} dyn_material_buffer;

void main()
{
    uint mat_id = get_material_id();
    pbr_material__gpu material = dyn_material_buffer.objects[mat_id];
    vec3 object_color = sample_bindless_texture(material.texture_indices[0], material.sampler_indices[0], in_tex_coord).xyz;
    out_color = vec4(object_color, 1.0);
}