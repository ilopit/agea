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

layout (location = 4) in vec2 in_lightmap_uv;
layout (location = 5) in flat uint in_object_idx;

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

// Single shadow map tap. hwPcf selects comparison sampler (bilinear [0,1])
// vs manual depth compare (hard 0/1).
float sampleShadow1(uint texIdx, vec2 uv, float compareDepth, uint hwPcf)
{
    if (hwPcf != 0u)
    {
        return texture(
            sampler2DShadow(bindless_textures[nonuniformEXT(texIdx)],
                            static_samplers[KGPU_SAMPLER_SHADOW_CMP]),
            vec3(uv, compareDepth));
    }
    float d = texture(
        sampler2D(bindless_textures[nonuniformEXT(texIdx)],
                  static_samplers[KGPU_SAMPLER_LINEAR_CLAMP_BORDER]),
        uv).r;
    return (compareDepth > d) ? 0.0 : 1.0;
}

// NxN grid PCF — samples clamped to tile rect to prevent atlas bleeding
float sampleShadowGrid(uint texIdx, vec2 uv, float compareDepth, float texelSize, int halfSize, vec2 uvMin, vec2 uvMax, uint hwPcf)
{
    float shadow = 0.0;
    float count = 0.0;
    for (int x = -halfSize; x <= halfSize; x++)
    {
        for (int y = -halfSize; y <= halfSize; y++)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            shadow += sampleShadow1(texIdx, clamp(uv + offset, uvMin, uvMax), compareDepth, hwPcf);
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

float sampleShadowPoisson16(uint texIdx, vec2 uv, float compareDepth, float texelSize, vec2 uvMin, vec2 uvMax, uint hwPcf)
{
    float shadow = 0.0;
    float spread = texelSize * 3.0;
    for (int i = 0; i < 16; i++)
    {
        shadow += sampleShadow1(texIdx, clamp(uv + poissonDisk16[i] * spread, uvMin, uvMax), compareDepth, hwPcf);
    }
    return shadow / 16.0;
}

float sampleShadowPoisson32(uint texIdx, vec2 uv, float compareDepth, float texelSize, vec2 uvMin, vec2 uvMax, uint hwPcf)
{
    float shadow = 0.0;
    float spread = texelSize * 4.0;
    for (int i = 0; i < 32; i++)
    {
        shadow += sampleShadow1(texIdx, clamp(uv + poissonDisk32[i] * spread, uvMin, uvMax), compareDepth, hwPcf);
    }
    return shadow / 32.0;
}

float sampleShadowPoisson64(uint texIdx, vec2 uv, float compareDepth, float texelSize, vec2 uvMin, vec2 uvMax, uint hwPcf)
{
    float shadow = 0.0;
    float spread = texelSize * 4.0;
    float innerC = 0.73900891f;
    float innerS = 0.67360556f;
    for (int i = 0; i < 32; i++)
    {
        vec2 p = poissonDisk32[i];
        shadow += sampleShadow1(texIdx, clamp(uv + p * spread, uvMin, uvMax), compareDepth, hwPcf);
        vec2 q = vec2(p.x * innerC - p.y * innerS, p.x * innerS + p.y * innerC);
        shadow += sampleShadow1(texIdx, clamp(uv + q * spread * 0.5, uvMin, uvMax), compareDepth, hwPcf);
    }
    return shadow / 64.0;
}

// Unified PCF dispatch for local lights
float sampleShadowPCF(uint texIdx, vec2 uv, float compareDepth, float texelSize, vec2 uvMin, vec2 uvMax, uint hwPcf)
{
    uint mode = dyn_shadow_data.shadow.directional.pcf_mode;

    if (mode == KGPU_PCF_5X5)
        return sampleShadowGrid(texIdx, uv, compareDepth, texelSize, 2, uvMin, uvMax, hwPcf);
    else if (mode == KGPU_PCF_7X7)
        return sampleShadowGrid(texIdx, uv, compareDepth, texelSize, 3, uvMin, uvMax, hwPcf);
    else if (mode == KGPU_PCF_POISSON16)
        return sampleShadowPoisson16(texIdx, uv, compareDepth, texelSize, uvMin, uvMax, hwPcf);
    else if (mode == KGPU_PCF_POISSON32)
        return sampleShadowPoisson32(texIdx, uv, compareDepth, texelSize, uvMin, uvMax, hwPcf);
    else if (mode == KGPU_PCF_POISSON64)
        return sampleShadowPoisson64(texIdx, uv, compareDepth, texelSize, uvMin, uvMax, hwPcf);
    else // KGPU_PCF_3X3 or default
        return sampleShadowGrid(texIdx, uv, compareDepth, texelSize, 1, uvMin, uvMax, hwPcf);
}

// Debug: set to 1 to visualize cascade indices as colors
#define SHADOW_DEBUG_CASCADE_VIS 0

// Single shadow tap for a cascade — project worldPos, compare depth, no PCF.
float sampleCascadeSingle(uint cascade, vec3 worldPos)
{
    mat4 lightVP = dyn_shadow_data.shadow.directional.cascades[cascade].view_proj;
    uint texIdx = dyn_shadow_data.shadow.atlas_bindless_index;
    vec2 uv_offset = dyn_shadow_data.shadow.directional.cascades[cascade].atlas_offset;
    vec2 uv_scale = dyn_shadow_data.shadow.directional.cascades[cascade].atlas_scale;

    vec4 lightSpacePos = lightVP * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    vec2 shadowUV = projCoords.xy * 0.5 + 0.5;
    float currentDepth = projCoords.z;

    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 || shadowUV.y < 0.0 || shadowUV.y > 1.0)
        return 1.0;
    if (currentDepth > 1.0 || currentDepth < 0.0)
        return 1.0;

    shadowUV = shadowUV * uv_scale + uv_offset;
    return sampleShadow1(texIdx, shadowUV, currentDepth, dyn_shadow_data.shadow.directional.hardware_pcf);
}

// CSM shadow with UV-space PCF. All offsets are in shadow map atlas UV —
// camera-independent, world-space radius controlled. The cascade is selected
// once per pixel; PCF samples stay within the same cascade tile.
float calcDirectionalShadow(vec3 worldPos, vec3 normal, float viewDepth)
{
    uint cascadeCount = dyn_shadow_data.shadow.directional.cascade_count;
    if (cascadeCount == 0u)
        return 1.0;

    float maxShadowDist = dyn_shadow_data.shadow.directional.cascades[cascadeCount - 1u].split_depth;
    if (viewDepth > maxShadowDist)
        return 1.0;

    float fadeStart = maxShadowDist * 0.9;
    float fadeFactor = 1.0;
    if (viewDepth > fadeStart)
        fadeFactor = 1.0 - (viewDepth - fadeStart) / (maxShadowDist - fadeStart);

    float normalBias = dyn_shadow_data.shadow.directional.normal_bias;
    vec3 biasedPos = worldPos + normal * normalBias;

    uint cascade = selectCascade(viewDepth);

    // Dithered cascade transition: near cascade boundaries, randomly select
    // the next cascade per pixel to avoid a hard seam.
    if (cascade < cascadeCount - 1u)
    {
        float splitDepth = dyn_shadow_data.shadow.directional.cascades[cascade].split_depth;
        float prevSplit = (cascade > 0u)
            ? dyn_shadow_data.shadow.directional.cascades[cascade - 1u].split_depth : 0.1;
        float cascadeRange = splitDepth - prevSplit;
        float blendZone = cascadeRange * 0.25;
        float distToEdge = splitDepth - viewDepth;

        if (distToEdge < blendZone && blendZone > 0.0)
        {
            float t = distToEdge / blendZone;
            float noise = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
            if (noise > t)
                cascade += 1u;
        }
    }

    mat4 lightVP = dyn_shadow_data.shadow.directional.cascades[cascade].view_proj;
    uint texIdx = dyn_shadow_data.shadow.atlas_bindless_index;
    vec2 uv_offset = dyn_shadow_data.shadow.directional.cascades[cascade].atlas_offset;
    vec2 uv_scale = dyn_shadow_data.shadow.directional.cascades[cascade].atlas_scale;

    vec3 pc = (lightVP * vec4(biasedPos, 1.0)).xyz;
    vec2 centerUV = pc.xy * 0.5 + 0.5;
    float centerDepth = pc.z;

    if (centerUV.x < 0.0 || centerUV.x > 1.0 || centerUV.y < 0.0 || centerUV.y > 1.0
        || centerDepth > 1.0 || centerDepth < 0.0)
        return 1.0;

    vec2 centerAtlasUV = centerUV * uv_scale + uv_offset;

    // Tile bounds with half-texel inset to prevent bilinear bleed at edges
    float halfTexel = dyn_shadow_data.shadow.directional.texel_size * 0.5;
    vec2 tileMin = uv_offset + halfTexel;
    vec2 tileMax = uv_offset + uv_scale - halfTexel;

    // World-space radius → atlas UV spread (camera-independent)
    float texelSize = dyn_shadow_data.shadow.directional.texel_size * uv_scale.x;
    float texelWorldSize = dyn_shadow_data.shadow.directional.cascades[cascade].texel_world_size;
    float worldRadius = dyn_shadow_data.shadow.directional.pcf_world_radius;
    float uvSpread = worldRadius / max(texelWorldSize, 1e-8) * texelSize;

    uint mode = dyn_shadow_data.shadow.directional.pcf_mode;
    uint hwPcf = dyn_shadow_data.shadow.directional.hardware_pcf;
    float shadow;

    // Grid modes: step = uvSpread / halfSize so total coverage = ±uvSpread (same as Poisson).
    // Poisson modes: spread multiplier already normalizes disk points to ±uvSpread.
    if (mode == KGPU_PCF_POISSON64)
        shadow = sampleShadowPoisson64(texIdx, centerAtlasUV, centerDepth, uvSpread / 4.0, tileMin, tileMax, hwPcf);
    else if (mode == KGPU_PCF_POISSON32)
        shadow = sampleShadowPoisson32(texIdx, centerAtlasUV, centerDepth, uvSpread / 4.0, tileMin, tileMax, hwPcf);
    else if (mode == KGPU_PCF_POISSON16)
        shadow = sampleShadowPoisson16(texIdx, centerAtlasUV, centerDepth, uvSpread / 3.0, tileMin, tileMax, hwPcf);
    else if (mode == KGPU_PCF_7X7)
        shadow = sampleShadowGrid(texIdx, centerAtlasUV, centerDepth, uvSpread / 3.0, 3, tileMin, tileMax, hwPcf);
    else if (mode == KGPU_PCF_5X5)
        shadow = sampleShadowGrid(texIdx, centerAtlasUV, centerDepth, uvSpread / 2.0, 2, tileMin, tileMax, hwPcf);
    else
        shadow = sampleShadowGrid(texIdx, centerAtlasUV, centerDepth, uvSpread, 1, tileMin, tileMax, hwPcf);

    return mix(1.0, shadow, fadeFactor);
}

// Calculate spot light shadow factor (atlas)
float calcSpotShadow(uint shadowIdx, vec3 worldPos)
{
    mat4 lightVP = dyn_shadow_data.shadow.local_shadows[shadowIdx].view_proj;
    uint texIdx = dyn_shadow_data.shadow.atlas_bindless_index;
    vec2 uv_offset = dyn_shadow_data.shadow.local_shadows[shadowIdx].atlas_offset_front;
    vec2 uv_scale = dyn_shadow_data.shadow.local_shadows[shadowIdx].atlas_scale_front;

    vec4 lightSpacePos = lightVP * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    vec2 shadowUV = projCoords.xy * 0.5 + 0.5;
    float currentDepth = projCoords.z;

    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 || shadowUV.y < 0.0 || shadowUV.y > 1.0)
        return 1.0;
    if (currentDepth > 1.0 || currentDepth < 0.0)
        return 1.0;

    shadowUV = shadowUV * uv_scale + uv_offset;

    float texelSize = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_params.z * uv_scale.x;
    float ht = texelSize * 0.5;
    vec2 spotMin = uv_offset + ht;
    vec2 spotMax = uv_offset + uv_scale - ht;
    return sampleShadowPCF(texIdx, shadowUV, currentDepth, texelSize, spotMin, spotMax, dyn_shadow_data.shadow.hardware_pcf_local);
}

// Calculate point light shadow factor using dual-paraboloid mapping (atlas)
float calcPointShadow(uint shadowIdx, vec3 worldPos, vec3 lightPos)
{
    vec3 lightToFrag = worldPos - lightPos;
    mat4 lightView = dyn_shadow_data.shadow.local_shadows[shadowIdx].view_proj;

    // Transform to light space
    vec3 L = (lightView * vec4(lightToFrag, 0.0)).xyz;

    // Select hemisphere — atlas UV from front or back tile
    bool frontFace = L.z >= 0.0;
    uint texIdx = dyn_shadow_data.shadow.atlas_bindless_index;
    vec2 uv_offset = frontFace
        ? dyn_shadow_data.shadow.local_shadows[shadowIdx].atlas_offset_front
        : dyn_shadow_data.shadow.local_shadows[shadowIdx].atlas_offset_back;
    vec2 uv_scale = frontFace
        ? dyn_shadow_data.shadow.local_shadows[shadowIdx].atlas_scale_front
        : dyn_shadow_data.shadow.local_shadows[shadowIdx].atlas_scale_back;

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

    // Remap to atlas coordinates
    uv = uv * uv_scale + uv_offset;

    float texelSize = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_params.z * uv_scale.x;
    float ht = texelSize * 0.5;
    vec2 ptMin = uv_offset + ht;
    vec2 ptMax = uv_offset + uv_scale - ht;
    return sampleShadowPCF(texIdx, uv, currentDepth, texelSize, ptMin, ptMax, dyn_shadow_data.shadow.hardware_pcf_local);
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
