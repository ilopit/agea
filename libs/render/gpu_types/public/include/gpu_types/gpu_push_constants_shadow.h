// Push constants for shadow passes (CSM + DPSM)
// Shared between C++ and GLSL

#ifndef GPU_PUSH_CONSTANTS_SHADOW_H
#define GPU_PUSH_CONSTANTS_SHADOW_H

#include <gpu_types/gpu_port.h>

#ifndef __cplusplus
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#endif

GPU_BEGIN_NAMESPACE

gpu_struct_pc push_constants_shadow
{
    uint instance_base;
    uint directional_light_id;
    uint use_clustered_lighting;
    pc_uint64 bdag_objects;
    pc_uint64 bdag_instance_slots;
    pc_uint64 bdag_shadow_data;
};

GPU_END_NAMESPACE

#endif
