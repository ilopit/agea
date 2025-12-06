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


layout(set = 2, binding = 0) uniform sampler2D tex1[];

void main()
{
    MaterialData material = dyn_material_buffer.objects[constants.material_id];
    PointLight light = dyn_point_lights_buffer.objects[0];
    // ambient
    vec3 ambient = light.ambient * material.ambient;

    // diffuse 
    vec3 norm = normalize(in_normal);
    vec3 lightDir = normalize(light.position.xyz - in_world_pos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.ambient.xyz * (diff * material.diffuse);
 
    // specular
    vec3 viewDir = normalize(dyn_camera_data.camPos - in_world_pos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.ambient.xyz * (spec * material.specular);  

    vec3 result = ambient + diffuse + specular;
    out_color = vec4(result, 1.0);
}