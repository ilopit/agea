// Common fragment shader — texture bindings + helpers
// Push constants, BDA extensions, and dyn_X macros must be declared by
// including shader BEFORE this file.
// Required macros (depending on pass):
//   dyn_camera_data, dyn_object_buffer, dyn_shadow_data,
//   dyn_cluster_config, dyn_cluster_light_counts, dyn_cluster_light_indices,
//   dyn_directional_lights_buffer, dyn_gpu_universal_light_data

#extension GL_EXT_nonuniform_qualifier : require
#include "gpu_types/gpu_generic_constants.h"

// Input
layout (location = 0) in vec3 in_world_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;

#ifdef KRYGA_LIGHTMAPPED
layout (location = 4) in vec2 in_lightmap_uv;
layout (location = 5) in flat uint in_object_idx;
#else
layout (location = 4) in flat uint in_object_idx;
#endif

// Output
layout (location = 0) out vec4 out_color;

// Static sampler array (set 2, binding 0) - immutable samplers for runtime selection
layout(set = KGPU_textures_descriptor_sets, binding = 0) uniform sampler static_samplers[KGPU_SAMPLER_COUNT];

// Bindless texture array (set 2, binding 1) - separate from samplers
// Note: Textures at binding 1 because variable descriptor count must be highest binding
layout(set = KGPU_textures_descriptor_sets, binding = 1) uniform texture2D bindless_textures[];

// Bindless texture sampling helper with runtime sampler selection
vec4 sample_bindless_texture(uint texture_idx, uint sampler_idx, vec2 uv)
{
    if (texture_idx == 0xFFFFFFFFu) // INVALID_BINDLESS_INDEX
        return vec4(1.0, 0.0, 1.0, 1.0); // Magenta for missing texture
    return texture(
        sampler2D(bindless_textures[nonuniformEXT(texture_idx)],
                  static_samplers[sampler_idx]),
        uv);
}

// Cluster lighting helper functions
uint getDepthSlice(float viewDepth)
{
    if (viewDepth <= dyn_cluster_config.config.near_plane)
        return 0u;

    float logDepth = log(viewDepth / dyn_cluster_config.config.near_plane);
    float t = logDepth / dyn_cluster_config.config.log_depth_ratio;

    // CRITICAL: enforce half-open range
    t = clamp(t, 0.0, 0.99999994);

    uint slice = uint(t * float(dyn_cluster_config.config.depth_slices));
    return min(slice, dyn_cluster_config.config.depth_slices - 1u);
}

uint getClusterIndex(vec2 screenPos, float viewDepth)
{
    uint tileX = uint(screenPos.x) / dyn_cluster_config.config.tile_size;
    uint tileY = uint(screenPos.y) / dyn_cluster_config.config.tile_size;
    uint slice = getDepthSlice(viewDepth);

    // Clamp to valid range
    tileX = min(tileX, dyn_cluster_config.config.tiles_x - 1u);
    tileY = min(tileY, dyn_cluster_config.config.tiles_y - 1u);
    slice = min(slice, dyn_cluster_config.config.depth_slices - 1u);

    return slice * (dyn_cluster_config.config.tiles_x * dyn_cluster_config.config.tiles_y)
         + tileY * dyn_cluster_config.config.tiles_x
         + tileX;
}

// Helper to get material_id from current object (uses in_object_idx from vertex shader)
uint get_material_id()
{
    return dyn_object_buffer.objects[in_object_idx].material_id;
}

// Lighting enable flags (from render config via push constants)
bool is_directional_light_enabled()
{
    return constants.obj.enable_directional_light != 0u;
}

bool is_local_lights_enabled()
{
    return constants.obj.enable_local_lights != 0u;
}

bool is_baked_light_enabled()
{
    return constants.obj.enable_baked_light != 0u;
}

// ============================================================================
// Shadow Mapping Helpers
// ============================================================================

// Select cascade by view-space depth
uint selectCascade(float viewDepth)
{
    for (uint i = 0u; i < dyn_shadow_data.shadow.directional.cascade_count; i++)
    {
        if (viewDepth < dyn_shadow_data.shadow.directional.cascades[i].split_depth)
        {
            return i;
        }
    }
    return dyn_shadow_data.shadow.directional.cascade_count - 1u;
}

// Single shadow map tap
float sampleShadow1(uint texIdx, vec2 uv, float compareDepth)
{
    float d = texture(
        sampler2D(bindless_textures[nonuniformEXT(texIdx)],
                  static_samplers[KGPU_SAMPLER_LINEAR_CLAMP_BORDER]),
        uv).r;
    return (compareDepth > d) ? 0.0 : 1.0;
}

// NxN grid PCF
float sampleShadowGrid(uint texIdx, vec2 uv, float compareDepth, float texelSize, int halfSize)
{
    float shadow = 0.0;
    float count = 0.0;
    for (int x = -halfSize; x <= halfSize; x++)
    {
        for (int y = -halfSize; y <= halfSize; y++)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            shadow += sampleShadow1(texIdx, uv + offset, compareDepth);
            count += 1.0;
        }
    }
    return shadow / count;
}

// Poisson disk samples (16 taps)
const vec2 poissonDisk16[16] = vec2[16](
    vec2(-0.94201624, -0.39906216), vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870), vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845), vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554), vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507), vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367), vec2( 0.14383161, -0.14100790)
);

// Poisson disk samples (32 taps)
const vec2 poissonDisk32[32] = vec2[32](
    vec2(-0.975402, -0.0711386), vec2(-0.920347, -0.4133930),
    vec2(-0.883908,  0.2178820), vec2(-0.793520, -0.6790080),
    vec2(-0.699150,  0.5831580), vec2(-0.613392,  0.1529960),
    vec2(-0.548550, -0.3342200), vec2(-0.498780, -0.8249640),
    vec2(-0.413776,  0.6921530), vec2(-0.308610, -0.0938680),
    vec2(-0.263900,  0.3563900), vec2(-0.209780, -0.6033800),
    vec2(-0.108820, -0.9396100), vec2(-0.051882,  0.1370340),
    vec2( 0.008170, -0.3291300), vec2( 0.066340,  0.8688200),
    vec2( 0.107060, -0.7162300), vec2( 0.177350,  0.5400620),
    vec2( 0.251830,  0.0839100), vec2( 0.290090, -0.4699020),
    vec2( 0.367930,  0.7368110), vec2( 0.432470, -0.1696800),
    vec2( 0.458040, -0.8480300), vec2( 0.536840,  0.3261600),
    vec2( 0.607640, -0.5501070), vec2( 0.651770,  0.0511700),
    vec2( 0.717160,  0.6038640), vec2( 0.770590, -0.2563180),
    vec2( 0.836530,  0.3614720), vec2( 0.880060, -0.6556600),
    vec2( 0.929550,  0.0423060), vec2( 0.978530, -0.3601700)
);

float sampleShadowPoisson16(uint texIdx, vec2 uv, float compareDepth, float texelSize)
{
    float shadow = 0.0;
    float spread = texelSize * 3.0;  // spread across 3 texels
    for (int i = 0; i < 16; i++)
    {
        shadow += sampleShadow1(texIdx, uv + poissonDisk16[i] * spread, compareDepth);
    }
    return shadow / 16.0;
}

float sampleShadowPoisson32(uint texIdx, vec2 uv, float compareDepth, float texelSize)
{
    float shadow = 0.0;
    float spread = texelSize * 4.0;  // spread across 4 texels
    for (int i = 0; i < 32; i++)
    {
        shadow += sampleShadow1(texIdx, uv + poissonDisk32[i] * spread, compareDepth);
    }
    return shadow / 32.0;
}

// Unified PCF dispatch — reads pcf_mode from shadow SSBO
float sampleShadowPCF(uint texIdx, vec2 uv, float compareDepth, float texelSize)
{
    uint mode = dyn_shadow_data.shadow.directional.pcf_mode;

    if (mode == KGPU_PCF_5X5)
        return sampleShadowGrid(texIdx, uv, compareDepth, texelSize, 2);
    else if (mode == KGPU_PCF_7X7)
        return sampleShadowGrid(texIdx, uv, compareDepth, texelSize, 3);
    else if (mode == KGPU_PCF_POISSON16)
        return sampleShadowPoisson16(texIdx, uv, compareDepth, texelSize);
    else if (mode == KGPU_PCF_POISSON32)
        return sampleShadowPoisson32(texIdx, uv, compareDepth, texelSize);
    else // KGPU_PCF_3X3 or default
        return sampleShadowGrid(texIdx, uv, compareDepth, texelSize, 1);
}

// Debug: set to 1 to visualize cascade indices as colors
#define SHADOW_DEBUG_CASCADE_VIS 0

// Sample shadow for a specific cascade
float sampleCascadeShadow(uint cascade, vec3 biasedPos)
{
    mat4 lightVP = dyn_shadow_data.shadow.directional.cascades[cascade].view_proj;
    uint texIdx = dyn_shadow_data.shadow.directional.shadow_map_indices[cascade];

    vec4 lightSpacePos = lightVP * vec4(biasedPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    vec2 shadowUV = projCoords.xy * 0.5 + 0.5;
    float currentDepth = projCoords.z;

    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 || shadowUV.y < 0.0 || shadowUV.y > 1.0)
        return 1.0;
    if (currentDepth > 1.0 || currentDepth < 0.0)
        return 1.0;

    float bias = dyn_shadow_data.shadow.directional.shadow_bias;
    currentDepth -= bias;

    float texelSize = dyn_shadow_data.shadow.directional.texel_size;
    return sampleShadowPCF(texIdx, shadowUV, currentDepth, texelSize);
}

// Calculate directional shadow factor using CSM with cascade blending
float calcDirectionalShadow(vec3 worldPos, vec3 normal, float viewDepth)
{
    uint cascadeCount = dyn_shadow_data.shadow.directional.cascade_count;
    if (cascadeCount == 0u)
        return 1.0;

    // Beyond the last cascade's split depth — no shadow
    float maxShadowDist = dyn_shadow_data.shadow.directional.cascades[cascadeCount - 1u].split_depth;
    if (viewDepth > maxShadowDist)
        return 1.0;

    // Fade out shadows near the distance limit (last 10% of range)
    float fadeStart = maxShadowDist * 0.9;
    float fadeFactor = 1.0;
    if (viewDepth > fadeStart)
        fadeFactor = 1.0 - (viewDepth - fadeStart) / (maxShadowDist - fadeStart);

    uint cascade = selectCascade(viewDepth);

    // Normal bias: offset world position along surface normal
    float normalBias = dyn_shadow_data.shadow.directional.normal_bias;
    vec3 biasedPos = worldPos + normal * normalBias;

    float shadow = sampleCascadeShadow(cascade, biasedPos);

    // Blend with next cascade near the split boundary to hide the resolution seam.
    // Blend zone is the last 10% of each cascade's depth range.
    if (cascade < cascadeCount - 1u)
    {
        float splitDepth = dyn_shadow_data.shadow.directional.cascades[cascade].split_depth;
        float prevSplit = (cascade > 0u)
            ? dyn_shadow_data.shadow.directional.cascades[cascade - 1u].split_depth
            : 0.1;
        float cascadeRange = splitDepth - prevSplit;
        float blendZone = cascadeRange * 0.1;
        float distToEdge = splitDepth - viewDepth;

        if (distToEdge < blendZone && blendZone > 0.0)
        {
            float nextShadow = sampleCascadeShadow(cascade + 1u, biasedPos);
            float t = distToEdge / blendZone;  // 1.0 at blend start, 0.0 at split boundary
            shadow = mix(nextShadow, shadow, t);
        }
    }

    // Fade to 1.0 (lit) near the shadow distance limit
    return mix(1.0, shadow, fadeFactor);
}

// Calculate spot light shadow factor
float calcSpotShadow(uint shadowIdx, vec3 worldPos)
{
    mat4 lightVP = dyn_shadow_data.shadow.local_shadows[shadowIdx].view_proj;
    uint texIdx = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_info.x;

    vec4 lightSpacePos = lightVP * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    vec2 shadowUV = projCoords.xy * 0.5 + 0.5;
    float currentDepth = projCoords.z;

    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 || shadowUV.y < 0.0 || shadowUV.y > 1.0)
        return 1.0;
    if (currentDepth > 1.0 || currentDepth < 0.0)
        return 1.0;

    float bias = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_params.x;
    currentDepth -= bias;

    float texelSize = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_params.z;
    return sampleShadowPCF(texIdx, shadowUV, currentDepth, texelSize);
}

// Calculate point light shadow factor using dual-paraboloid mapping
float calcPointShadow(uint shadowIdx, vec3 worldPos, vec3 lightPos)
{
    vec3 lightToFrag = worldPos - lightPos;
    mat4 lightView = dyn_shadow_data.shadow.local_shadows[shadowIdx].view_proj;

    // Transform to light space
    vec3 L = (lightView * vec4(lightToFrag, 0.0)).xyz;

    // Select hemisphere
    bool frontFace = L.z >= 0.0;
    uint texIdx = frontFace
        ? dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_info.x
        : dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_info.y;

    // Flip for back hemisphere
    if (!frontFace)
        L.z = -L.z;

    // Paraboloid projection
    float len = length(L);
    L /= len;
    vec2 uv = L.xy / (L.z + 1.0) * 0.5 + 0.5;

    // Linear depth
    float nearPlane = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_params.w;
    float farPlane = dyn_shadow_data.shadow.local_shadows[shadowIdx].far_plane;
    float currentDepth = (len - nearPlane) / (farPlane - nearPlane);

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;
    if (currentDepth > 1.0 || currentDepth < 0.0)
        return 1.0;

    float bias = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_params.x;
    currentDepth -= bias;

    float texelSize = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_params.z;
    return sampleShadowPCF(texIdx, uv, currentDepth, texelSize);
}

// Get shadow factor for a local light based on its shadow_index
float getLocalLightShadow(universal_light_data light, vec3 worldPos)
{
    if (light.shadow_index == KGPU_SHADOW_INDEX_NONE)
        return 1.0;

    uint shadowIdx = light.shadow_index;
    if (shadowIdx >= dyn_shadow_data.shadow.shadowed_local_count)
        return 1.0;

    uint lightType = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_info.z;
    if (lightType == KGPU_light_type_spot)
        return calcSpotShadow(shadowIdx, worldPos);
    else
        return calcPointShadow(shadowIdx, worldPos, light.position);
}
