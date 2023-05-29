#version 450

#include "common_frag.glsl" 

// materials
struct MaterialData
{
    vec3 color;
};

//all object matrices
layout(std140, set = 1, binding = 1) readonly buffer MaterialBuffer{   

    MaterialData objects[];
} dyn_material_buffer;


layout(set = 2, binding = 0) uniform sampler2D tex1[2];

void main()
{
    MaterialData material = dyn_material_buffer.objects[constants.material_id];

    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}