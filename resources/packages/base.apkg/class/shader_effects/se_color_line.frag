#version 450

#include "common_frag.glsl"

layout (location = 0) in vec3 inFragPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inTexCoord;


// materials
struct MaterialData
{
    vec3 color;
};

//all object matrices
layout(std140, set = 1, binding = 1) readonly buffer MaterialBuffer{   

    MaterialData objects[];
} dyn_material_buffer;


layout(set = 2, binding = 0) uniform sampler2D tex1;

void main()
{
    MaterialData material = dyn_material_buffer.objects[constants.material_id];

    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}