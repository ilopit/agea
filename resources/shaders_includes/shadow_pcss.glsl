// PCSS (Percentage-Closer Soft Shadows) — directional only. Fully self-contained
// soft-shadow path with its OWN bias (pcss_*). Part of common_frag.glsl; include
// after shadow_pcf.glsl (uses sampleShadow1 + the Poisson disks).

// ============================================================================
// PCSS (Percentage-Closer Soft Shadows) — directional only.
// Fully self-contained: own bias (pcss_bias / pcss_normal_bias) and own blocker
// search. Does NOT touch calcDirectionalShadow or any sampleShadow* PCF helper,
// so tuning PCSS can never regress the fixed-radius PCF modes.
// ============================================================================

// Raw (non-comparison) depth fetch for the blocker search. The PCF path uses the
// comparison sampler (0/1 verdict); PCSS must read the STORED depth to average
// blocker distances, so it samples the linear/border depth view directly. Border
// = 1.0 (far) outside the tile, so out-of-tile taps read as "no blocker".
float pcss_fetchDepth(uint texIdx, vec2 uv)
{
    return texture(
        sampler2D(bindless_textures[nonuniformEXT(texIdx)],
                  static_samplers[KGPU_SAMPLER_LINEAR_CLAMP_BORDER]),
        uv).r;
}

// Stage 1: average depth of texels closer to the light than the receiver.
// Returns -1.0 when no blocker found (fully lit). searchUV = radius in atlas UV.
float pcss_blockerSearch(uint texIdx, vec2 uv, float receiverDepth,
                         float searchUV, vec2 tileMin, vec2 tileMax)
{
    float sum = 0.0;
    float count = 0.0;
    for (int i = 0; i < 16; i++)
    {
        vec2 p = clamp(uv + poissonDisk16[i] * searchUV, tileMin, tileMax);
        float d = pcss_fetchDepth(texIdx, p);
        if (d < receiverDepth)
        {
            sum += d;
            count += 1.0;
        }
    }
    return count > 0.0 ? sum / count : -1.0;
}

// Stage 3: variable-radius PCF using the comparison sampler (Poisson-32).
float pcss_filter(uint texIdx, vec2 uv, float compareDepth, float spreadUV,
                  vec2 tileMin, vec2 tileMax, uint hwPcf)
{
    float shadow = 0.0;
    for (int i = 0; i < 32; i++)
    {
        shadow += sampleShadow1(texIdx,
                                clamp(uv + poissonDisk32[i] * spreadUV, tileMin, tileMax),
                                compareDepth, hwPcf);
    }
    return shadow / 32.0;
}

// PCSS directional shadow. Mirrors calcDirectionalShadow's projection/fade/cascade
// setup but with its OWN bias and a blocker-search-driven variable kernel.
float calcDirectionalShadowPCSS(vec3 worldPos, vec3 normal, float viewDepth)
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

    // PCSS-OWN bias — independent of the PCF shadow_bias / normal_bias fields.
    float normalBias = dyn_shadow_data.shadow.directional.pcss_normal_bias;
    float depthBias = dyn_shadow_data.shadow.directional.pcss_bias;
    vec3 biasedPos = worldPos + normal * normalBias
                     - dyn_shadow_data.shadow.directional.light_dir.xyz * depthBias;

    uint cascade = selectCascade(viewDepth);

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

    float halfTexel = dyn_shadow_data.shadow.directional.texel_size * 0.5;
    vec2 tileMin = uv_offset + halfTexel;
    vec2 tileMax = uv_offset + uv_scale - halfTexel;

    // Light size (world) -> atlas UV, using the same world->UV mapping as PCF.
    float texelSize = dyn_shadow_data.shadow.directional.texel_size * uv_scale.x;
    float texelWorldSize = dyn_shadow_data.shadow.directional.cascades[cascade].texel_world_size;
    float lightSize = dyn_shadow_data.shadow.directional.pcss_light_size;
    float lightUV = lightSize / max(texelWorldSize, 1e-8) * texelSize;

    uint hwPcf = dyn_shadow_data.shadow.directional.hardware_pcf;

    // Stage 1: blocker search.
    float blocker = pcss_blockerSearch(texIdx, centerAtlasUV, centerDepth, lightUV, tileMin, tileMax);
    if (blocker < 0.0)
        return mix(1.0, 1.0, fadeFactor);  // no blocker -> fully lit

    // Stage 2: penumbra ratio (similar triangles; ortho depth is linear in world Z).
    float penumbra = (centerDepth - blocker) / max(blocker, 1e-5);
    // Clamp kernel to [1 texel, lightUV] — never zero (avoids hard aliasing), never
    // larger than the light-size footprint (bounds cost + tile bleed).
    float spreadUV = clamp(penumbra * lightUV, texelSize, lightUV);

    // Stage 3: variable-radius PCF.
    float shadow = pcss_filter(texIdx, centerAtlasUV, centerDepth, spreadUV, tileMin, tileMax, hwPcf);

    return mix(1.0, shadow, fadeFactor);
}
