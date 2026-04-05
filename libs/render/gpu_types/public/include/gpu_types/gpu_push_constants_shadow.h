// Push constants for shadow passes (CSM + DPSM)
// Shared between C++ and GLSL

#ifndef GPU_PUSH_CONSTANTS_SHADOW_H
#define GPU_PUSH_CONSTANTS_SHADOW_H

#include <gpu_types/gpu_port.h>

#ifndef __cplusplus
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#endif

GPU_BEGIN_NAMESPACE

push_struct push_constants_shadow
{
    push_uint instance_base;
    push_uint directional_light_id;
    push_uint use_clustered_lighting;
    push_uint64 bdag_objects;
    push_uint64 bdag_instance_slots;
    push_uint64 bdag_shadow_data;
};

GPU_END_NAMESPACE

#endif
