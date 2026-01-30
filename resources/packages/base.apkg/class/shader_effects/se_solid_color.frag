#version 450
#include "common_frag.glsl"

// materials
struct MaterialData
{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

//all object matrices
layout(std140, set = 3, binding = 0) readonly buffer MaterialBuffer{

    MaterialData objects[];
} dyn_material_buffer;

void main()
{
    MaterialData material = dyn_material_buffer.objects[constants.obj.material_id];

    // Simple solid color output using material diffuse
    out_color = vec4(material.diffuse, 1.0);
}