// GPU Object Types - Shared between C++ and GLSL

#ifndef GPU_OBJECT_TYPES_H
#define GPU_OBJECT_TYPES_H

#include <gpu_types/gpu_port.h>

GPU_BEGIN_NAMESPACE

// Object data for rendering - uses type-aware alignment macros
gpu_struct_std140 object_data
{
    std140_mat4  model;
    std140_mat4  normal;
    std140_vec3  obj_pos;
    std140_float bounding_radius;  // Type-safe: std140_float applies correct 4-byte alignment
};

GPU_END_NAMESPACE

#endif  // GPU_OBJECT_TYPES_H
