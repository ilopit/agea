// Local-light shadows — spot (single tile) and point (dual-paraboloid) lookups plus
// the per-light dispatch. Part of common_frag.glsl; include after shadow_pcf.glsl
// (uses sampleShadowPCF).

// Calculate spot light shadow factor (atlas)
float calcSpotShadow(uint shadowIdx, vec3 worldPos, vec3 normal)
{
    mat4 lightVP = dyn_shadow_data.shadow.local_shadows[shadowIdx].view_proj;
    uint texIdx = dyn_shadow_data.shadow.atlas_bindless_index;
    vec2 uv_offset = dyn_shadow_data.shadow.local_shadows[shadowIdx].atlas_offset_front;
    vec2 uv_scale = dyn_shadow_data.shadow.local_shadows[shadowIdx].atlas_scale_front;

    // shadow_params: x = depth bias, y = normal bias (shadows.bias / normal_bias).
    float depthBias = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_params.x;
    float normalBias = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_params.y;
    worldPos += normal * normalBias;

    vec4 lightSpacePos = lightVP * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    vec2 shadowUV = projCoords.xy * 0.5 + 0.5;
    float currentDepth = projCoords.z;

    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 || shadowUV.y < 0.0 || shadowUV.y > 1.0)
        return 1.0;
    if (currentDepth > 1.0 || currentDepth < 0.0)
        return 1.0;

    currentDepth -= depthBias;
    shadowUV = shadowUV * uv_scale + uv_offset;

    float texelSize = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_params.z * uv_scale.x;
    float ht = texelSize * 0.5;
    vec2 spotMin = uv_offset + ht;
    vec2 spotMax = uv_offset + uv_scale - ht;
    return sampleShadowPCF(texIdx, shadowUV, currentDepth, texelSize, spotMin, spotMax, dyn_shadow_data.shadow.hardware_pcf_local);
}

// Calculate point light shadow factor using dual-paraboloid mapping (atlas)
float calcPointShadow(uint shadowIdx, vec3 worldPos, vec3 lightPos, vec3 normal)
{
    // shadow_params: x = depth bias, y = normal bias (shadows.bias / normal_bias).
    float depthBias = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_params.x;
    float normalBias = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_params.y;

    vec3 lightToFrag = (worldPos + normal * normalBias) - lightPos;
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

    currentDepth -= depthBias;

    // Remap to atlas coordinates
    uv = uv * uv_scale + uv_offset;

    float texelSize = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_params.z * uv_scale.x;
    float ht = texelSize * 0.5;
    vec2 ptMin = uv_offset + ht;
    vec2 ptMax = uv_offset + uv_scale - ht;
    return sampleShadowPCF(texIdx, uv, currentDepth, texelSize, ptMin, ptMax, dyn_shadow_data.shadow.hardware_pcf_local);
}

// Get shadow factor for a local light based on its shadow_index
float getLocalLightShadow(universal_light_data light, vec3 worldPos, vec3 normal)
{
    if (light.shadow_index == KGPU_SHADOW_INDEX_NONE)
        return 1.0;

    uint shadowIdx = light.shadow_index;
    if (shadowIdx >= dyn_shadow_data.shadow.shadowed_local_count)
        return 1.0;

    uint lightType = dyn_shadow_data.shadow.local_shadows[shadowIdx].shadow_info.z;
    if (lightType == KGPU_light_type_spot)
        return calcSpotShadow(shadowIdx, worldPos, normal);
    else
        return calcPointShadow(shadowIdx, worldPos, light.position, normal);
}
