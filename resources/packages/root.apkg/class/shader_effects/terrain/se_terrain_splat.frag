#version 450
#extension GL_GOOGLE_include_directive: enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "gpu_types/gpu_push_constants_main.h"
layout(push_constant, scalar) uniform Constants { push_constants_main obj; } constants;
#include "bda_macros_main.glsl"
#include "common_frag.glsl"

#include "gpu_types/terrain_splatmap_material__gpu.h"
layout(buffer_reference, scalar) readonly buffer BdaMaterialBuffer {
    terrain_splatmap_material__gpu objects[];
};
#define dyn_material_buffer BdaMaterialBuffer(constants.obj.bdaf_material)

// Splat-blended albedo replaces the per-material diffuse constant, so the
// lighting helpers take it as a parameter (the lit materials read diffuse from
// the material buffer instead).
vec3 terrainDirLight(directional_light_data light, vec3 normal, vec3 viewDir,
                     uint mat_idx, vec3 albedo, float shadow)
{
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0),
                     dyn_material_buffer.objects[mat_idx].shininess);

    vec3 ambient  = light.ambient  * dyn_material_buffer.objects[mat_idx].ambient * albedo;
    vec3 diffuse  = light.diffuse  * diff * albedo;
    vec3 specular = light.specular * spec * dyn_material_buffer.objects[mat_idx].specular;

    return ambient + (diffuse + specular) * shadow;
}

vec3 terrainPointLight(universal_light_data light, vec3 normal, vec3 fragPos,
                       vec3 viewDir, uint mat_idx, vec3 albedo)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    if (diff < 0.0001) return vec3(0);

    float distance = length(light.position - fragPos);
    float d_ratio = distance / light.radius;
    if (d_ratio >= 1.0) return vec3(0);

    float d_ratio2 = d_ratio * d_ratio;
    float falloff = 1.0 / (1.0 + d_ratio2);
    float d_ratio4 = d_ratio2 * d_ratio2;
    float window = clamp(1.0 - d_ratio4, 0.0, 1.0);
    window = window * window;
    float attenuation = falloff * window;

    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0),
                     dyn_material_buffer.objects[mat_idx].shininess);

    vec3 diffuse  = light.diffuse  * diff * albedo;
    vec3 specular = light.specular * spec * dyn_material_buffer.objects[mat_idx].specular;

    return (diffuse + specular) * attenuation;
}

vec3 terrainSpotLight(universal_light_data light, vec3 normal, vec3 fragPos,
                      vec3 viewDir, uint mat_idx, vec3 albedo)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    if (diff < 0.0001) return vec3(0);

    float distance = length(light.position - fragPos);
    float d_ratio = distance / light.radius;
    if (d_ratio >= 1.0) return vec3(0);

    float d_ratio2 = d_ratio * d_ratio;
    float falloff = 1.0 / (1.0 + d_ratio2);
    float d_ratio4 = d_ratio2 * d_ratio2;
    float window = clamp(1.0 - d_ratio4, 0.0, 1.0);
    window = window * window;
    float attenuation = falloff * window;

    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0),
                     dyn_material_buffer.objects[mat_idx].shininess);

    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cut_off - light.outer_cut_off;
    float intensity = clamp((theta - light.outer_cut_off) / epsilon, 0.0, 1.0);

    vec3 diffuse  = light.diffuse  * diff * albedo;
    vec3 specular = light.specular * spec * dyn_material_buffer.objects[mat_idx].specular;

    return (diffuse + specular) * attenuation * intensity;
}

void main()
{
    vec3 norm = normalize(in_normal);
    vec3 viewDir = normalize(dyn_camera_data.obj.position - in_world_pos);
    uint _mi = get_material_id();

    // Splatmap weights (no tiling — spans the whole terrain), clamped sampler.
    vec4 w = sample_bindless_texture(
        dyn_material_buffer.objects[_mi].texture_indices[KGPU_TEXTURE_SLOT_SPLATMAP],
        KGPU_SAMPLER_LINEAR_CLAMP, in_tex_coord);

    float total = w.r + w.g + w.b + w.a;
    w = (total > 1e-4) ? w / total : vec4(1.0, 0.0, 0.0, 0.0);

    vec4 tiling = dyn_material_buffer.objects[_mi].layer_tiling;
    vec3 l0 = sample_bindless_texture(
        dyn_material_buffer.objects[_mi].texture_indices[KGPU_TEXTURE_SLOT_LAYER0],
        KGPU_SAMPLER_LINEAR_REPEAT, in_tex_coord * tiling.x).rgb;
    vec3 l1 = sample_bindless_texture(
        dyn_material_buffer.objects[_mi].texture_indices[KGPU_TEXTURE_SLOT_LAYER1],
        KGPU_SAMPLER_LINEAR_REPEAT, in_tex_coord * tiling.y).rgb;
    vec3 l2 = sample_bindless_texture(
        dyn_material_buffer.objects[_mi].texture_indices[KGPU_TEXTURE_SLOT_LAYER2],
        KGPU_SAMPLER_LINEAR_REPEAT, in_tex_coord * tiling.z).rgb;
    vec3 l3 = sample_bindless_texture(
        dyn_material_buffer.objects[_mi].texture_indices[KGPU_TEXTURE_SLOT_LAYER3],
        KGPU_SAMPLER_LINEAR_REPEAT, in_tex_coord * tiling.w).rgb;

    vec3 albedo = l0 * w.r + l1 * w.g + l2 * w.b + l3 * w.a;

    vec4 viewPos = dyn_camera_data.obj.view * vec4(in_world_pos, 1.0);
    float viewDepth = -viewPos.z;
    float dirShadow = calcDirectionalShadow(in_world_pos, norm, viewDepth);

    vec3 result = vec3(0);
    if (is_directional_light_enabled())
        result += terrainDirLight(
            dyn_directional_lights_buffer.objects[constants.obj.directional_light_id],
            norm, viewDir, _mi, albedo, dirShadow);

    if (is_local_lights_enabled())
    {
        uint clusterIdx = getClusterIndex(gl_FragCoord.xy, viewDepth);
        uint lightCount = dyn_cluster_light_counts.objects[clusterIdx].count;
        uint baseIdx = clusterIdx * dyn_cluster_config.config.max_lights_per_cluster;

        for (uint i = 0u; i < lightCount; i++)
        {
            uint lightSlot = dyn_cluster_light_indices.objects[baseIdx + i].index;
            universal_light_data light = dyn_gpu_universal_light_data.objects[lightSlot];
            float localShadow = getLocalLightShadow(light, in_world_pos, norm);

            if (light.type == KGPU_light_type_point)
                result += terrainPointLight(light, norm, in_world_pos, viewDir, _mi, albedo) * localShadow;
            else if (light.type == KGPU_light_type_spot)
                result += terrainSpotLight(light, norm, in_world_pos, viewDir, _mi, albedo) * localShadow;
        }
    }

    out_color = vec4(result, 1.0);
}
