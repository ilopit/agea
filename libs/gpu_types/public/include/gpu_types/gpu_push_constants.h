// GPU Push Constants - Shared between C++ and GLSL

#ifndef GPU_PUSH_CONSTANTS_H
#define GPU_PUSH_CONSTANTS_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

gpu_struct_pc push_constants
{
    uint material_id;
    uint directional_light_id;
    uint local_lights_size;
    uint local_light_ids[8];
};

GPU_END_NAMESPACE

#endif  // GPU_PUSH_CONSTANTS_H
