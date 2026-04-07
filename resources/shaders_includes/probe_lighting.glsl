// Runtime light probe sampling for dynamic objects
// Include after common_frag.glsl (needs dyn_probe_data, dyn_probe_grid, dyn_object_buffer)

#include "bake/sh_common.glsl"

// Evaluate indirect irradiance from the nearest probe assigned to this object
vec3 evaluate_probe_lighting(uint object_idx, vec3 normal)
{
    uint probe_idx = dyn_object_buffer.objects[object_idx].probe_index;

    // No probe assigned
    if (probe_idx == 0xFFFFFFFFu)
        return vec3(0.0);

    if (probe_idx >= dyn_probe_grid.grid.probe_count)
        return vec3(0.0);

    return sh_evaluate(dyn_probe_data.probes[probe_idx], normal);
}

// Evaluate indirect irradiance with trilinear interpolation across 8 grid neighbors
vec3 evaluate_probe_lighting_interpolated(vec3 world_pos, vec3 normal)
{
    probe_grid_config g = dyn_probe_grid.grid;

    if (g.probe_count == 0u || g.spacing < 0.001)
        return vec3(0.0);

    // Find grid cell
    vec3 rel = (world_pos - g.grid_min) / g.spacing;
    vec3 f = fract(rel);
    ivec3 base = ivec3(floor(rel));

    // Clamp to grid bounds
    base = clamp(base, ivec3(0), ivec3(g.grid_size_x - 2u, g.grid_size_y - 2u, g.grid_size_z - 2u));

    vec3 result = vec3(0.0);
    float weight_sum = 0.0;

    // Trilinear interpolation over 8 corners
    for (int dz = 0; dz <= 1; dz++)
    {
        for (int dy = 0; dy <= 1; dy++)
        {
            for (int dx = 0; dx <= 1; dx++)
            {
                ivec3 cell = base + ivec3(dx, dy, dz);
                uint idx = uint(cell.z) * g.grid_size_x * g.grid_size_y
                         + uint(cell.y) * g.grid_size_x
                         + uint(cell.x);

                if (idx >= g.probe_count)
                    continue;

                float wx = (dx == 0) ? (1.0 - f.x) : f.x;
                float wy = (dy == 0) ? (1.0 - f.y) : f.y;
                float wz = (dz == 0) ? (1.0 - f.z) : f.z;
                float w = wx * wy * wz;

                result += sh_evaluate(dyn_probe_data.probes[idx], normal) * w;
                weight_sum += w;
            }
        }
    }

    return (weight_sum > 0.0) ? result / weight_sum : vec3(0.0);
}
