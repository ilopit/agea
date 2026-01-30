// GPU Push Constants - Shared between C++ and GLSL

#ifndef GPU_PUSH_CONSTANTS_H
#define GPU_PUSH_CONSTANTS_H

#include <gpu_types/gpu_port.h>
#include <gpu_types/gpu_generic_constants.h>

GPU_BEGIN_NAMESPACE

// Standard texture slots for bindless indexing
// Note: Keep slot count low to fit within push constant limits (128-256 bytes)
#define KGPU_TEXTURE_SLOT_ALBEDO    0
#define KGPU_MAX_TEXTURE_SLOTS      1

gpu_struct_pc push_constants
{
    uint material_id;
    uint directional_light_id;
    uint use_clustered_lighting;  // 0 = per-object light grid, 1 = clustered
    uint instance_base;           // base offset into instance_slots buffer for this batch
    uint local_lights_size;       // number of lights for per-object grid path
    uint texture_indices[KGPU_MAX_TEXTURE_SLOTS];     // bindless texture indices
    uint sampler_indices[KGPU_MAX_TEXTURE_SLOTS];     // sampler indices into static_samplers array
    uint local_light_ids[KGPU_max_lights_per_object];  // light indices for per-object grid path
};

GPU_END_NAMESPACE

#endif  // GPU_PUSH_CONSTANTS_H
