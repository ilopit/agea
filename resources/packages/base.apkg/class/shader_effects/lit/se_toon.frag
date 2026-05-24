#version 450
#extension GL_GOOGLE_include_directive: enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require

layout(constant_id = 0) const bool ENABLE_LIGHTMAP = false;

#include "gpu_types/gpu_push_constants_main.h"
layout(push_constant, scalar) uniform Constants { push_constants_main obj; } constants;
#include "bda_macros_main.glsl"
#include "common_frag.glsl"

#include "gpu_types/toon_material__gpu.h"
layout(buffer_reference, scalar) readonly buffer BdaMaterialBuffer {
    toon_material__gpu objects[];
};
#define dyn_material_buffer BdaMaterialBuffer(constants.obj.bdaf_material)

// Quantize a [0,1] value into N discrete bands with a narrow smoothstep
// at each boundary to avoid 1-pixel aliasing on nearly-aligned surfaces.
float quantize_band(float v, float bands)
{
    float scaled = clamp(v, 0.0, 1.0) * bands;
    float stepped = floor(scaled);
    float frac = scaled - stepped;
    float edge = smoothstep(0.92, 1.0, frac);
    return (stepped + edge) / bands;
}

float toon_diffuse(vec3 normal, vec3 lightDir, float bands)
{
    float ndl = max(dot(normal, lightDir), 0.0);
    return quantize_band(ndl, bands);
}

// Stepped specular with a hard cutoff. Returns 0 when spec_strength == 0.
float toon_specular(vec3 normal, vec3 viewDir, vec3 lightDir, float shininess, float spec_strength)
{
    if (spec_strength <= 0.0) return 0.0;
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), max(shininess, 8.0));
    return smoothstep(0.48, 0.52, spec) * spec_strength;
}

vec3 toonDirLight(directional_light_data light, vec3 normal, vec3 viewDir,
                  vec3 albedo, float shadow,
                  float bands, float shininess, float spec_strength)
{
    vec3 lightDir = normalize(-light.direction);
    float diff = toon_diffuse(normal, lightDir, bands);
    float spec = toon_specular(normal, viewDir, lightDir, shininess, spec_strength);

    vec3 ambient  = light.ambient * albedo;
    vec3 diffuse  = light.diffuse * diff * albedo;
    vec3 specular = light.specular * spec;

    return ambient + (diffuse + specular) * shadow;
}

vec3 toonPointLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir,
                    vec3 albedo,
                    float bands, float shininess, float spec_strength)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float raw_ndl = max(dot(normal, lightDir), 0.0);
    if (raw_ndl < 0.0001) return vec3(0);

    float distance = length(light.position - fragPos);
    float d_ratio = distance / light.radius;
    if (d_ratio >= 1.0) return vec3(0);

    float d_ratio2 = d_ratio * d_ratio;
    float falloff = 1.0 / (1.0 + d_ratio2);
    float d_ratio4 = d_ratio2 * d_ratio2;
    float window = clamp(1.0 - d_ratio4, 0.0, 1.0);
    window = window * window;
    float attenuation = quantize_band(falloff * window, bands);

    float diff = quantize_band(raw_ndl, bands);
    float spec = toon_specular(normal, viewDir, lightDir, shininess, spec_strength);

    vec3 diffuse  = light.diffuse * diff * albedo;
    vec3 specular = light.specular * spec;
    return (diffuse + specular) * attenuation;
}

vec3 toonSpotLight(universal_light_data light, vec3 normal, vec3 fragPos, vec3 viewDir,
                   vec3 albedo,
                   float bands, float shininess, float spec_strength)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float raw_ndl = max(dot(normal, lightDir), 0.0);
    if (raw_ndl < 0.0001) return vec3(0);

    float distance = length(light.position - fragPos);
    float d_ratio = distance / light.radius;
    if (d_ratio >= 1.0) return vec3(0);

    float d_ratio2 = d_ratio * d_ratio;
    float falloff = 1.0 / (1.0 + d_ratio2);
    float d_ratio4 = d_ratio2 * d_ratio2;
    float window = clamp(1.0 - d_ratio4, 0.0, 1.0);
    window = window * window;
    float attenuation = quantize_band(falloff * window, bands);

    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cut_off - light.outer_cut_off;
    float intensity = clamp((theta - light.outer_cut_off) / epsilon, 0.0, 1.0);
    intensity = quantize_band(intensity, max(bands - 1.0, 2.0));

    float diff = quantize_band(raw_ndl, bands);
    float spec = toon_specular(normal, viewDir, lightDir, shininess, spec_strength);

    vec3 diffuse  = light.diffuse * diff * albedo;
    vec3 specular = light.specular * spec;
    return (diffuse + specular) * attenuation * intensity;
}

void main()
{
    uint _mi = get_material_id();
    float bands = max(dyn_material_buffer.objects[_mi].band_count, 2.0);
    float shininess = dyn_material_buffer.objects[_mi].shininess;
    float spec_strength = dyn_material_buffer.objects[_mi].specular_strength;

    vec3 albedo = sample_bindless_texture(
        dyn_material_buffer.objects[_mi].texture_indices[KGPU_TEXTURE_SLOT_ALBEDO],
        dyn_material_buffer.objects[_mi].sampler_indices[KGPU_TEXTURE_SLOT_ALBEDO],
        in_tex_coord).rgb;

    vec3 norm = normalize(in_normal);
    vec3 viewDir = normalize(dyn_camera_data.obj.position - in_world_pos);

    vec4 viewPos = dyn_camera_data.obj.view * vec4(in_world_pos, 1.0);
    float viewDepth = -viewPos.z;

    float dirShadow = calcDirectionalShadow(in_world_pos, norm, viewDepth);
    // Snap shadow to two bands — either lit or not, no soft falloff.
    dirShadow = step(0.5, dirShadow);

    vec3 result = vec3(0);
    if (is_directional_light_enabled())
    {
        result += toonDirLight(
            dyn_directional_lights_buffer.objects[constants.obj.directional_light_id],
            norm, viewDir, albedo, dirShadow,
            bands, shininess, spec_strength);
    }

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
            localShadow = step(0.5, localShadow);

            if (light.type == KGPU_light_type_point)
                result += toonPointLight(light, norm, in_world_pos, viewDir, albedo, bands, shininess, spec_strength) * localShadow;
            else if (light.type == KGPU_light_type_spot)
                result += toonSpotLight(light, norm, in_world_pos, viewDir, albedo, bands, shininess, spec_strength) * localShadow;
        }
    }

    out_color = vec4(result, 1.0);
}
