#version 450
#extension GL_GOOGLE_include_directive: enable

layout(constant_id = 0) const bool ENABLE_LIGHTMAP = false;

#include "gpu_types/gpu_push_constants_main.h"
layout(push_constant) uniform Constants { push_constants_main obj; } constants;
#include "descriptor_bindings_common.glsl"
#include "common_frag.glsl"

#include "gpu_types/pbr_material__gpu.h"
layout(set = KGPU_materials_descriptor_sets, binding = 0, scalar) readonly buffer MaterialBuffer
{
    pbr_material__gpu objects[];
} dyn_material_buffer;

#include "lightmap_sampling.glsl"

// Forward declarations
vec3 CalcDirLight(directional_light_data light, vec3 normal, vec3 viewDir, float shininess, vec3 albedo, vec3 specular_tex, float shadow);
vec3 CalcDirLightDirect(directional_light_data light, vec3 normal, vec3 viewDir, float shininess, vec3 albedo, vec3 specular_tex, float shadow);
vec3 CalcPointLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir, float shininess, vec3 albedo, vec3 specular_tex);
vec3 CalcSpotLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir, float shininess, vec3 albedo, vec3 specular_tex);

void main()
{
    uint _mi = get_material_id();
    float shininess = dyn_material_buffer.objects[_mi].shininess;

    vec3 albedo = sample_bindless_texture(dyn_material_buffer.objects[_mi].texture_indices[KGPU_TEXTURE_SLOT_ALBEDO], dyn_material_buffer.objects[_mi].sampler_indices[KGPU_TEXTURE_SLOT_ALBEDO], in_tex_coord).rgb;
    vec3 specular_tex = sample_bindless_texture(dyn_material_buffer.objects[_mi].texture_indices[KGPU_TEXTURE_SLOT_SPECULAR], dyn_material_buffer.objects[_mi].sampler_indices[KGPU_TEXTURE_SLOT_SPECULAR], in_tex_coord).rgb;

    vec3 norm = normalize(in_normal);
    vec3 viewDir = normalize(dyn_camera_data.obj.position - in_world_pos);

    vec4 viewPos = dyn_camera_data.obj.view * vec4(in_world_pos, 1.0);
    float viewDepth = -viewPos.z;

    float dirShadow = calcDirectionalShadow(in_world_pos, norm, viewDepth);

    if (ENABLE_LIGHTMAP)
    {
        // Sample lightmap: baked indirect illumination (per-instance texture from object_data)
        vec3 baked_gi = vec3(0);
        if (is_baked_light_enabled())
            baked_gi = sample_lightmap(
                in_lightmap_uv,
                dyn_object_buffer.objects[in_object_idx].lightmap_texture_index,
                KGPU_SAMPLER_LINEAR_CLAMP);

        // Realtime direct lighting only (no ambient -- lightmap replaces it)
        vec3 direct = vec3(0);
        if (is_directional_light_enabled())
            direct = CalcDirLightDirect(
                dyn_directional_lights_buffer.objects[constants.obj.directional_light_id],
                norm, viewDir, shininess, albedo, specular_tex, dirShadow);

        // Local lights (point and spot) -- still fully realtime
        if (is_local_lights_enabled())
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
                    direct += CalcPointLight(light, norm, in_world_pos, viewDir, shininess, albedo, specular_tex) * localShadow;
                else if(light.type == KGPU_light_type_spot)
                    direct += CalcSpotLight(light, norm, in_world_pos, viewDir, shininess, albedo, specular_tex) * localShadow;
            }
        }

        // Combine: baked GI (indirect) + realtime direct
        vec3 result = blend_baked_and_realtime(baked_gi * albedo, direct);
        out_color = vec4(apply_dither(result, gl_FragCoord.xy), 1.0);
    }
    else
    {
        // phase 1: directional lighting
        vec3 result = vec3(0);
        if (is_directional_light_enabled())
            result += CalcDirLight(dyn_directional_lights_buffer.objects[constants.obj.directional_light_id], norm, viewDir, shininess, albedo, specular_tex, dirShadow);

        // phase 2: local lights (point and spot)
        if (is_local_lights_enabled())
        {
            uint clusterIdx = getClusterIndex(gl_FragCoord.xy, viewDepth);

            uint lightCount = dyn_cluster_light_counts.objects[clusterIdx].count;
            uint baseIdx = clusterIdx * dyn_cluster_config.config.max_lights_per_cluster;

            // Iterate over lights in this cluster
            for (uint i = 0u; i < lightCount; i++)
            {
                uint lightSlot = dyn_cluster_light_indices.objects[baseIdx + i].index;
                universal_light_data light = dyn_gpu_universal_light_data.objects[lightSlot];
                float localShadow = getLocalLightShadow(light, in_world_pos);

                if(light.type == KGPU_light_type_point)
                {
                    result += CalcPointLight(light, norm, in_world_pos, viewDir, shininess, albedo, specular_tex) * localShadow;
                }
                else if(light.type == KGPU_light_type_spot)
                {
                    result += CalcSpotLight(light, norm, in_world_pos, viewDir, shininess, albedo, specular_tex) * localShadow;
                }
            }
        }

        out_color = vec4(apply_dither(result, gl_FragCoord.xy), 1.0);
    }
}

// Directional light with ambient (non-lightmapped path)
vec3 CalcDirLight(directional_light_data light, vec3 normal, vec3 viewDir, float shininess, vec3 albedo, vec3 specular_tex, float shadow)
{
    vec3 lightDir = normalize(-light.direction);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    // combine results
    vec3 ambient = light.ambient * albedo;
    vec3 diffuse = light.diffuse * diff * albedo;
    vec3 specular = light.specular * spec * specular_tex;

    return ambient + (diffuse + specular) * shadow;
}

// Directional light: direct contribution only (no ambient -- lightmap provides it)
vec3 CalcDirLightDirect(directional_light_data light, vec3 normal, vec3 viewDir, float shininess, vec3 albedo, vec3 specular_tex, float shadow)
{
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);

    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);

    vec3 diffuse = light.diffuse * diff * albedo;
    vec3 specular = light.specular * spec * specular_tex;

    return (diffuse + specular) * shadow;
}


// calculates the color when using a point light.
vec3 CalcPointLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir, float shininess, vec3 albedo, vec3 specular_tex)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    if(diff < 0.0001)
    {
        return vec3(0);
    }

    float distance = length(light.position - fragPos);
    float d_ratio = distance / light.radius;

    // Early out if beyond radius
    if(d_ratio >= 1.0)
    {
        return vec3(0);
    }

    // Inverse-square falloff with steeper curve
    float d_ratio2 = d_ratio * d_ratio;
    float falloff = 1.0 / (1.0 + 25.0 * d_ratio2);

    // UE4-style window function to smoothly fade to zero at radius
    float d_ratio4 = d_ratio2 * d_ratio2;
    float window = clamp(1.0 - d_ratio4, 0.0, 1.0);
    window = window * window;

    float attenuation = falloff * window;

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);

    // combine results
    vec3 ambient = light.ambient * albedo;
    vec3 diffuse = light.diffuse * diff * albedo;
    vec3 specular = light.specular * spec * specular_tex;

    return (ambient + diffuse + specular) * attenuation;
}

// calculates the color when using a spot light.
vec3 CalcSpotLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir, float shininess, vec3 albedo, vec3 specular_tex)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    if(diff < 0.0001)
    {
        return vec3(0);
    }

    float distance = length(light.position - fragPos);
    float d_ratio = distance / light.radius;

    // Early out if beyond radius
    if(d_ratio >= 1.0)
    {
        return vec3(0);
    }

    // Inverse-square falloff with steeper curve
    float d_ratio2 = d_ratio * d_ratio;
    float falloff = 1.0 / (1.0 + 25.0 * d_ratio2);

    // UE4-style window function to smoothly fade to zero at radius
    float d_ratio4 = d_ratio2 * d_ratio2;
    float window = clamp(1.0 - d_ratio4, 0.0, 1.0);
    window = window * window;

    float attenuation = falloff * window;

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);

    // spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cut_off - light.outer_cut_off;
    float intensity = clamp((theta - light.outer_cut_off) / epsilon, 0.0, 1.0);

    // combine results
    vec3 ambient = light.ambient * albedo;
    vec3 diffuse = light.diffuse * diff * albedo;
    vec3 specular = light.specular * spec * specular_tex;

    return (ambient + diffuse + specular) * attenuation * intensity;
}
