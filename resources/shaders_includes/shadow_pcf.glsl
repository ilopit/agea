// Shadow sampling primitives — cascade selection + the fixed-radius PCF taps
// (single tap, NxN grid, Poisson 16/32/64) and the unified local-light PCF dispatch.
// Part of common_frag.glsl; include after frag_core.glsl. Consumed by the PCSS,
// directional, and local shadow modules.

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
