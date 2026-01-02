#version 450
#extension GL_GOOGLE_include_directive : require
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
layout(std140, set = KGPU_materials_descriptor_sets, binding = 0) readonly buffer MaterialBuffer{

    MaterialData objects[];
} dyn_material_buffer;

// Forward declarations
vec3 CalcDirLight(directional_light_data light, vec3 normal, vec3 viewDir, MaterialData material);
vec3 CalcPointLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir, MaterialData material);
vec3 CalcSpotLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir, MaterialData material);

void main()
{
    // properties
    vec3 norm = normalize(in_normal);
    vec3 viewDir = normalize(dyn_camera_data.obj.position - in_world_pos);
    MaterialData material = dyn_material_buffer.objects[constants.obj.material_id];

    // phase 1: directional lighting
    vec3 result = vec3(0);
    result += CalcDirLight(dyn_directional_lights_buffer.objects[constants.obj.directional_light_id], norm, viewDir, material);

    // phase 2: local lights (point and spot)
    for(uint i = 0; i < constants.obj.local_lights_size; i++)
    {
        universal_light_data light = dyn_gpu_universal_light_data.objects[constants.obj.local_light_ids[i]];
        if(light.type == KGPU_light_type_point)
        {
            result += CalcPointLight(light, norm, in_world_pos, viewDir, material);
        }
        else if(light.type == KGPU_light_type_spot)
        {
            result += CalcSpotLight(light, norm, in_world_pos, viewDir, material);
        }
    }

    out_color = vec4(result, 1.0);
}

// calculates the color when using a directional light.
vec3 CalcDirLight(directional_light_data light, vec3 normal, vec3 viewDir, MaterialData material)
{
    vec3 lightDir = normalize(-light.direction);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // combine results
    vec3 ambient = light.ambient * material.ambient;
    vec3 diffuse = light.diffuse * diff * material.diffuse;
    vec3 specular = light.specular * spec * material.specular;

    return (ambient + diffuse + specular);
}


// calculates the color when using a point light.
vec3 CalcPointLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir, MaterialData material)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    if(diff < 0.0001)
    {
        return vec3(0);
    }

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    // combine results
    vec3 ambient = light.ambient * material.ambient;
    vec3 diffuse = light.diffuse * diff * material.diffuse;
    vec3 specular = light.specular * spec * material.specular;
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return (ambient + diffuse + specular);
}

// calculates the color when using a spot light.
vec3 CalcSpotLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir, MaterialData material)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    if(diff < 0.0001)
    {
        return vec3(0);
    }

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    // spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cut_off - light.outer_cut_off;
    float intensity = clamp((theta - light.outer_cut_off) / epsilon, 0.0, 1.0);
    // combine results
    vec3 ambient = light.ambient * material.ambient;
    vec3 diffuse = light.diffuse * diff * material.diffuse;
    vec3 specular = light.specular * spec * material.specular;
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;

    return (ambient + diffuse + specular);
}
