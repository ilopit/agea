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


layout(set = 2, binding = 0) uniform sampler2D tex1[1];

void main()
{
    vec3 object_color = texture(tex1[0], inTexCoord).xyz;
    outColor = vec4(object_color, 1.0);
}