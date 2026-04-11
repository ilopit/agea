#version 450
#extension GL_GOOGLE_include_directive: enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "gpu_types/gpu_push_constants_main.h"
layout(push_constant) uniform Constants { push_constants_main obj; } constants;
#include "bda_macros_main.glsl"
#define KRYGA_LIGHTMAPPED
#include "common_frag.glsl"

#include "gpu_types/solid_color_material__gpu.h"
layout(buffer_reference, scalar) readonly buffer BdaMaterialBuffer {
    solid_color_material__gpu objects[];
};
#define dyn_material_buffer BdaMaterialBuffer(constants.obj.bdaf_material)

#include "lightmap_sampling.glsl"

// Forward declarations
vec3 CalcDirLightDirect(directional_light_data light, vec3 normal, vec3 viewDir, uint mat_idx, float shadow);
vec3 CalcPointLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir, uint mat_idx);
vec3 CalcSpotLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir, uint mat_idx);

void main()
{
    vec3 norm = normalize(in_normal);
    vec3 viewDir = normalize(dyn_camera_data.obj.position - in_world_pos);
    uint _mi = get_material_id();

    vec3 albedo = dyn_material_buffer.objects[_mi].diffuse;

    vec4 viewPos = dyn_camera_data.obj.view * vec4(in_world_pos, 1.0);
    float viewDepth = -viewPos.z;

    float dirShadow = calcDirectionalShadow(in_world_pos, norm, viewDepth);

    // Sample lightmap: baked indirect illumination (per-instance texture from object_data)
    vec3 baked_gi = vec3(0);
    if (is_baked_light_enabled())
        baked_gi = sample_lightmap(
            in_lightmap_uv,
            dyn_object_buffer.objects[in_object_idx].lightmap_texture_index,
            KGPU_SAMPLER_LINEAR_CLAMP);

    // Realtime direct lighting only (no ambient — lightmap replaces it)
    vec3 direct = vec3(0);
    if (is_directional_light_enabled())
        direct = CalcDirLightDirect(
            dyn_directional_lights_buffer.objects[constants.obj.directional_light_id],
            norm, viewDir, _mi, dirShadow);

    // Local lights (point and spot) — still fully realtime
    if (is_local_lights_enabled() && constants.obj.use_clustered_lighting != 0u)
    {
        uint clusterIdx = getClusterIndex(gl_FragCoord.xy, viewDepth);
        uint lightCount = dyn_cluster_light_counts.objects[clusterIdx].count;
        uint baseIdx = clusterIdx * dyn_cluster_config.config.max_lights_per_cluster;

        for (uint i = 0u; i < lightCount; i++)
        {
            uint lightSlot = dyn_cluster_light_indices.objects[baseIdx + i].index;
            universal_light_data light = dyn_gpu_universal_light_data.objects[lightSlot];
            float localShadow = getLocalLightShadow(light, in_world_pos);

            if(light.type == KGPU_light_type_point)
                direct += CalcPointLight(light, norm, in_world_pos, viewDir, _mi) * localShadow;
            else if(light.type == KGPU_light_type_spot)
                direct += CalcSpotLight(light, norm, in_world_pos, viewDir, _mi) * localShadow;
        }
    }
    else if (is_local_lights_enabled())
    {
        for (uint i = 0u; i < constants.obj.local_lights_size; i++)
        {
            universal_light_data light = dyn_gpu_universal_light_data.objects[constants.obj.local_light_ids[i]];
            float localShadow = getLocalLightShadow(light, in_world_pos);

            if(light.type == KGPU_light_type_point)
                direct += CalcPointLight(light, norm, in_world_pos, viewDir, _mi) * localShadow;
            else if(light.type == KGPU_light_type_spot)
                direct += CalcSpotLight(light, norm, in_world_pos, viewDir, _mi) * localShadow;
        }
    }

    // Combine: baked GI (indirect) + realtime direct
    vec3 result = blend_baked_and_realtime(baked_gi * albedo, direct);
    out_color = vec4(result, 1.0);
}

// Directional light: direct contribution only (no ambient — lightmap provides it)
vec3 CalcDirLightDirect(directional_light_data light, vec3 normal, vec3 viewDir, uint mat_idx, float shadow)
{
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);

    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), dyn_material_buffer.objects[mat_idx].shininess);

    vec3 diffuse = light.diffuse * diff * dyn_material_buffer.objects[mat_idx].diffuse;
    vec3 specular = light.specular * spec * dyn_material_buffer.objects[mat_idx].specular;

    return (diffuse + specular) * shadow;
}

// Point light
vec3 CalcPointLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir, uint mat_idx)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    if(diff < 0.0001) return vec3(0);

    float distance = length(light.position - fragPos);
    float d_ratio = distance / light.radius;
    if(d_ratio >= 1.0) return vec3(0);

    float d_ratio2 = d_ratio * d_ratio;
    float falloff = 1.0 / (1.0 + 25.0 * d_ratio2);
    float d_ratio4 = d_ratio2 * d_ratio2;
    float window = clamp(1.0 - d_ratio4, 0.0, 1.0);
    window = window * window;
    float attenuation = falloff * window;

    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), dyn_material_buffer.objects[mat_idx].shininess);

    vec3 diffuse = light.diffuse * diff * dyn_material_buffer.objects[mat_idx].diffuse;
    vec3 specular = light.specular * spec * dyn_material_buffer.objects[mat_idx].specular;

    return (diffuse + specular) * attenuation;
}

// Spot light
vec3 CalcSpotLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir, uint mat_idx)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    if(diff < 0.0001) return vec3(0);

    float distance = length(light.position - fragPos);
    float d_ratio = distance / light.radius;
    if(d_ratio >= 1.0) return vec3(0);

    float d_ratio2 = d_ratio * d_ratio;
    float falloff = 1.0 / (1.0 + 25.0 * d_ratio2);
    float d_ratio4 = d_ratio2 * d_ratio2;
    float window = clamp(1.0 - d_ratio4, 0.0, 1.0);
    window = window * window;
    float attenuation = falloff * window;

    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), dyn_material_buffer.objects[mat_idx].shininess);

    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cut_off - light.outer_cut_off;
    float intensity = clamp((theta - light.outer_cut_off) / epsilon, 0.0, 1.0);

    vec3 diffuse = light.diffuse * diff * dyn_material_buffer.objects[mat_idx].diffuse;
    vec3 specular = light.specular * spec * dyn_material_buffer.objects[mat_idx].specular;

    return (diffuse + specular) * attenuation * intensity;
}
