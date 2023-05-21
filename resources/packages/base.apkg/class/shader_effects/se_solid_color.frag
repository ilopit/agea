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
layout(std140, set = 1, binding = 1) readonly buffer MaterialBuffer{   

    MaterialData objects[];
} dyn_material_buffer;


layout(set = 2, binding = 0) uniform sampler2D tex1[];

void main()
{
    MaterialData material = dyn_material_buffer.objects[constants.material_id];

    // ambient
    vec3 ambient = dyn_scene_data.ambient * material.ambient;

    // diffuse 
    vec3 norm = normalize(inNormal);
    vec3 lightDir = normalize(dyn_scene_data.position.xyz - inFragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = dyn_scene_data.ambient.xyz * (diff * material.diffuse);

    // specular
    vec3 viewDir = normalize(dyn_camera_data.camPos - inFragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = dyn_scene_data.ambient.xyz * (spec * material.specular);  

    vec3 result = ambient + diffuse + specular;
    outColor = vec4(result, 1.0);
}