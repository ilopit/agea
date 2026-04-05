// Push constants for grid pass
// Shared between C++ and GLSL

#ifndef GPU_PUSH_CONSTANTS_GRID_H
#define GPU_PUSH_CONSTANTS_GRID_H

#include <gpu_types/gpu_port.h>

#ifndef __cplusplus
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#endif

GPU_BEGIN_NAMESPACE

gpu_struct_pc push_constants_grid
{
    pc_uint64 bdag_camera;
};

GPU_END_NAMESPACE

#endif
