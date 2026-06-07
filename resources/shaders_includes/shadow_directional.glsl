// Directional CSM shadow entry point — selects PCSS vs the fixed-radius PCF modes,
// dithered cascade blending, world-space bias. Part of common_frag.glsl; include
// after shadow_pcf.glsl and shadow_pcss.glsl (calls into both).

// CSM shadow with UV-space PCF. All offsets are in shadow map atlas UV —
// camera-independent, world-space radius controlled. The cascade is selected
// once per pixel; PCF samples stay within the same cascade tile.
float calcDirectionalShadow(vec3 worldPos, vec3 normal, float viewDepth)
{
    // PCSS is a fully separate path — dispatch out before any PCF logic runs.
    if (dyn_shadow_data.shadow.directional.pcf_mode == KGPU_PCF_PCSS)
        return calcDirectionalShadowPCSS(worldPos, normal, viewDepth);

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

    // Normal bias (along surface normal) + world-space depth bias (along the light
    // direction, toward the light). shadow_bias is in METERS, so a single value yields the
    // same world-space offset in every cascade — no peter-pan jump at cascade transitions.
    float normalBias = dyn_shadow_data.shadow.directional.normal_bias;
    float depthBias = dyn_shadow_data.shadow.directional.shadow_bias;
    vec3 biasedPos = worldPos + normal * normalBias
                     - dyn_shadow_data.shadow.directional.light_dir.xyz * depthBias;

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

    // (Depth bias already applied in world space via biasedPos above — see top of fn.)
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
