// GPU Push Constants - Shared between C++ and GLSL

#ifndef GPU_PUSH_CONSTANTS_H
#define GPU_PUSH_CONSTANTS_H

#include <gpu_types/gpu_port.h>
#include <gpu_types/gpu_generic_constants.h>

GPU_BEGIN_NAMESPACE

gpu_struct_pc push_constants
{
    uint material_id;
    uint directional_light_id;
    uint local_lights_size;
    uint local_light_ids[KGPU_max_lights_per_object];
};

GPU_END_NAMESPACE

#endif  // GPU_PUSH_CONSTANTS_H
