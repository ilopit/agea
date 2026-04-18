// Push constants for shadow passes (CSM + DPSM)
// Shared between C++ and GLSL

#ifndef GPU_PUSH_CONSTANTS_SHADOW_H
#define GPU_PUSH_CONSTANTS_SHADOW_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

struct push_constants_shadow
{
    uint instance_base;
    uint directional_light_id;
    uint use_clustered_lighting;
};

GPU_END_NAMESPACE

#endif
