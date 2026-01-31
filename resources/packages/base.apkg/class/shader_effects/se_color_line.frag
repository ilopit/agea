#version 450

#include "common_frag.glsl"

// materials
struct MaterialData
{
    vec3 color;
};

//all object matrices
layout(std140, set = 3, binding = 0) readonly buffer MaterialBuffer{

    MaterialData objects[];
} dyn_material_buffer;

void main()
{
    // Use per-object material_id from object buffer
    MaterialData material = dyn_material_buffer.objects[get_material_id()];

    out_color = vec4(1.0, 1.0, 1.0, 1.0);
}