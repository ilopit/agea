#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

layout (set = 2, binding = 0) uniform sampler static_samplers[7];
layout (set = 2, binding = 1) uniform texture2D bindless_textures[];

layout (push_constant) uniform OutlineParams {
    vec4 outline_color;
    vec2 texel_size;
    float thickness;
    uint mask_texture_idx;
} params;

float sample_mask(vec2 uv)
{
    // Use alpha channel — opaque objects always output 1.0,
    // background is 0.0. Immune to lighting darkness.
    return texture(sampler2D(bindless_textures[nonuniformEXT(params.mask_texture_idx)],
                             static_samplers[1]),
                   uv).a;
}

void main()
{
    float c = sample_mask(in_uv);

    float edge = 0.0;
    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            if (dx == 0 && dy == 0)
                continue;

            vec2 offset = vec2(float(dx), float(dy)) * params.texel_size * params.thickness;
            float n = sample_mask(in_uv + offset);

            if (abs(c - n) > 0.1)
            {
                edge = 1.0;
            }
        }
    }

    if (edge > 0.0)
    {
        out_color = params.outline_color;
    }
    else
    {
        discard;
    }
}
