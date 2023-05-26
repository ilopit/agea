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


layout(set = 2, binding = 0) uniform sampler2D tex1[2];

// function prototypes
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);

void main()
{    
    // properties
    vec3 norm = normalize(inNormal);
    vec3 viewDir = normalize(dyn_camera_data.camPos - inFragPos);
    
    // == =====================================================
    // Our lighting is set up in 3 phases: directional, point lights and an optional flashlight
    // For each phase, a calculate function is defined that calculates the corresponding color
    // per lamp. In the main() function we take all the calculated colors and sum them up for
    // this fragment's final color.
    // == =====================================================
    // phase 1: directional lighting
    vec3 result = vec3(0);
    
    result += CalcDirLight(dyn_directional_lights_buffer.objects[constants.directional_light_id], norm, viewDir);
    // phase 2: point lights
    for(int i = 0; i < constants.point_lights_size; i++)
    {
        result += CalcPointLight(dyn_point_lights_buffer.objects[i], norm, inFragPos, viewDir);    
    }

    for(int i = 0; i < constants.spot_lights_size; i++)
    {
        result += CalcSpotLight(dyn_spot_lights_buffer.objects[i], norm, inFragPos, viewDir);    
    }

     outColor = vec4(result, 1.0);
}


// calculates the color when using a directional light.
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
    MaterialData material = dyn_material_buffer.objects[constants.material_id];

    vec3 lightDir = normalize(-light.direction);
    
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
    // combine results
    vec3 ambient = light.ambient * vec3(texture(tex1[0], inTexCoord));
    vec3 diffuse = light.diffuse * diff * vec3(texture(tex1[0], inTexCoord));
    vec3 specular = light.specular * spec * vec3(texture(tex1[1], inTexCoord));

    return (ambient + diffuse + specular);
}


// calculates the color when using a point light.
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
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
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    // combine results
    vec3 ambient = light.ambient * vec3(texture(tex1[0], inTexCoord));
    vec3 diffuse = light.diffuse * diff * vec3(texture(tex1[0], inTexCoord));
    vec3 specular = light.specular * spec * vec3(texture(tex1[1], inTexCoord));
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return (ambient + diffuse + specular);
}
// calculates the color when using a spot light.
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
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
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    // spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction)); 
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    // combine results
    vec3 ambient = light.ambient * vec3(texture(tex1[0], inTexCoord));
    vec3 diffuse = light.diffuse * diff * vec3(texture(tex1[0], inTexCoord));
    vec3 specular = light.specular * spec * vec3(texture(tex1[1], inTexCoord));
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;

    return (ambient + diffuse + specular);
}