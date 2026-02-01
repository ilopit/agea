#version 450

#include "common_frag.glsl"

struct MaterialData
{
    uint texture_indices[KGPU_MAX_TEXTURE_SLOTS];
    uint sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
    vec3 color;
};

layout(std430, set = KGPU_materials_descriptor_sets, binding = 0) readonly buffer MaterialBuffer{
    MaterialData objects[];
} dyn_material_buffer;

void main()
{
    // Use per-object material_id from object buffer
    MaterialData material = dyn_material_buffer.objects[get_material_id()];

    out_color = vec4(1.0, 1.0, 1.0, 1.0);
}