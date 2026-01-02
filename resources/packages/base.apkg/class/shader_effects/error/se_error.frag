#version 450
#extension GL_GOOGLE_include_directive: enable
#include "common_frag.glsl"

const vec3 COLOR[4] = vec3[4] (
    vec3 (0.1, 0.3, 1),
    vec3(1, 0.3, 0.1),
    vec3(1, 0.3, 0.1),
    vec3(0.1, 0.3, 1) );

vec3 getColor(vec2 uv)
{
    int vx = mod(uv.x, 0.2) > 0.1 ? 1: 0;
    int vy = mod(uv.y, 0.2) > 0.1 ? 1: 0;

    return COLOR[vx*2 + vy];
}

vec3 CalcDummyDirLight(vec3 normal, vec3 fragPos, vec3 viewDir)
{
    directional_light_data light;

    light.direction = vec3(0, -1, 0.5);
    light.ambient = vec3(1.0);
    light.diffuse = vec3(1.0);
    light.specular = vec3(1.0);

    vec3 lightDir = normalize(-light.direction);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    // combine results
    vec3 ambient = light.ambient * getColor(in_tex_coord);
    vec3 diffuse = light.diffuse * diff * getColor(in_tex_coord);
    vec3 specular = light.specular * spec * getColor(in_tex_coord);

    return ambient + diffuse + specular;
}

// calculates the color when using a point light.
vec3 CalcDummyPointLight(vec3 normal, vec3 fragPos, vec3 viewDir)
{
    universal_light_data light;

    light.position = dyn_camera_data.obj.position;
    light.ambient = vec3(1.0);
    light.diffuse = vec3(1.0);
    light.specular = vec3(1.0);

    light.constant = 2;
    light.linear = 0.014;
    light.quadratic = 0.0007;

    vec3 lightDir = normalize(dyn_camera_data.obj.position - fragPos);
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
    vec3 ambient = light.ambient * getColor(in_tex_coord);
    vec3 diffuse = light.diffuse * diff * getColor(in_tex_coord);
    vec3 specular = light.specular * spec * getColor(in_tex_coord);
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return (ambient + diffuse + specular);
}

void main()
{
    vec3 norm = normalize(in_normal);
    vec3 view_dir = normalize(dyn_camera_data.obj.position - in_world_pos);

    vec3 result = CalcDummyDirLight(norm, in_world_pos, view_dir);
    result += CalcDummyPointLight(norm, in_world_pos, view_dir);

    out_color = vec4(result, 1.0);
}
