// Push constants for picking pass
// Shared between C++ and GLSL

#ifndef GPU_PUSH_CONSTANTS_PICK_H
#define GPU_PUSH_CONSTANTS_PICK_H

#include <gpu_types/gpu_port.h>

#ifndef __cplusplus
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#endif

GPU_BEGIN_NAMESPACE

gpu_struct_pc push_constants_pick
{
    uint instance_base;
    pc_uint64 bdag_camera;
    pc_uint64 bdag_objects;
    pc_uint64 bdag_instance_slots;
    pc_uint64 bdag_bone_matrices;
};

GPU_END_NAMESPACE

#endif
