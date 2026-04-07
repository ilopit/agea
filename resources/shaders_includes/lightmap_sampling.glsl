// Lightmap sampling utilities
// Include after common_frag.glsl (needs sample_bindless_texture)

// Sample the lightmap texture using lightmap UVs
vec3 sample_lightmap(vec2 lightmap_uv, uint lightmap_tex_idx, uint lightmap_sampler_idx)
{
    if (lightmap_tex_idx == 0xFFFFFFFFu)
        return vec3(0.0);

    return sample_bindless_texture(lightmap_tex_idx, lightmap_sampler_idx, lightmap_uv).rgb;
}

// Blend baked indirect illumination with realtime direct lighting.
// baked_gi:  indirect illumination from lightmap (already includes bounced light)
// direct:    realtime direct lighting contribution (diffuse + specular, with shadows)
// Returns the combined result.
vec3 blend_baked_and_realtime(vec3 baked_gi, vec3 direct)
{
    return baked_gi + direct;
}
