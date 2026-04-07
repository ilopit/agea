// GPU Light Probe Types - Shared between C++ and GLSL
// SH L2 (order 2) spherical harmonic probes for baked indirect lighting

#ifndef GPU_PROBE_TYPES_H
#define GPU_PROBE_TYPES_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

// SH L2 probe: 9 coefficients per RGB channel = 27 floats
// Packed into 7 vec4s (28 floats, 27 used + 1 padding)
struct sh_probe
{
    // SH coefficients packed as vec4s for efficient GPU access
    // [0] = L0 (R, G, B, L1_R_-1)
    // [1] = (L1_R_0, L1_R_1, L1_G_-1, L1_G_0)
    // [2] = (L1_G_1, L1_B_-1, L1_B_0, L1_B_1)
    // [3] = (L2_R_-2, L2_R_-1, L2_R_0, L2_R_1)
    // [4] = (L2_R_2, L2_G_-2, L2_G_-1, L2_G_0)
    // [5] = (L2_G_1, L2_G_2, L2_B_-2, L2_B_-1)
    // [6] = (L2_B_0, L2_B_1, L2_B_2, _pad)
    vec4 coefficients[7];

    vec3 position;
    float radius;
};

// Probe grid configuration (scalar layout)
struct probe_grid_config
{
    vec3 grid_min;         // world-space AABB minimum
    float spacing;         // distance between probes
    vec3 grid_max;         // world-space AABB maximum
    uint probe_count;      // total number of active probes
    uint grid_size_x;      // probes per axis
    uint grid_size_y;
    uint grid_size_z;
};

GPU_END_NAMESPACE

#endif // GPU_PROBE_TYPES_H
